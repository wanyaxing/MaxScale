/*
 * Copyright (c) 2018 MariaDB Corporation Ab
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file and at www.mariadb.com/bsl11.
 *
 * Change Date: 2022-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2 or later of the General
 * Public License.
 */

#include <maxscale/config2.hh>
#include "internal/config.hh"

using namespace std;

namespace config
{

/**
 * class Specification
 */
Specification::Specification(const char* zModule)
    : m_module(zModule)
{
}

Specification::~Specification()
{
}

const string& Specification::module() const
{
    return m_module;
}

const Param* Specification::find_param(const string& name) const
{
    auto it = m_params.find(name);

    return it != m_params.end() ? it->second : nullptr;
}

ostream& Specification::document(ostream& out) const
{
    for (const auto& entry : m_params)
    {
        out << entry.second->documentation() << endl;
    }

    return out;
}

bool Specification::validate(const MXS_CONFIG_PARAMETER& params) const
{
    bool valid = true;

    set<string> provided;

    for (const auto& param : params)
    {
        const auto& name = param.first;
        const auto& value = param.second;

        const Param* pParam = find_param(name.c_str());

        if (pParam)
        {
            string message;

            if (!pParam->validate(value.c_str(), &message))
            {
                valid = false;
            }

            if (!message.empty())
            {
                if (valid)
                {
                    MXS_WARNING("%s: %s", name.c_str(), message.c_str());
                }
                else
                {
                    MXS_ERROR("%s: %s", name.c_str(), message.c_str());
                }
            }

            provided.insert(name);
        }
        else
        {
            MXS_WARNING("%s: The parameter '%s' is unrecognized.", m_module.c_str(), name.c_str());
            valid = false;
        }
    }

    for (const auto& entry : m_params)
    {
        const Param* pParam = entry.second;

        if (pParam->is_mandatory() && (provided.find(pParam->name()) == provided.end()))
        {
            MXS_ERROR("%s: The mandatory parameter '%s' is not provided.",
                      m_module.c_str(), pParam->name().c_str());
            valid = false;
        }
    }

    return valid;
}

bool Specification::configure(Configuration& configuration, const MXS_CONFIG_PARAMETER& params) const
{
    mxb_assert(validate(params));
    mxb_assert(size() == configuration.size());

    bool configured = true;

    for (const auto& param : params)
    {
        const auto& name = param.first;
        const auto& value = param.second;

        const Param* pParam = find_param(name.c_str());
        config::Type* pValue = configuration.find_value(name.c_str());

        mxb_assert(pValue && pParam); // Should have been validated.
        mxb_assert(&pValue->parameter() == pParam);

        if (pParam && pValue)
        {
            if (!pParam->set(*pValue, value.c_str()))
            {
                mxb_assert(!true);
                configured = false;
            }
        }
        else
        {
            MXS_ERROR("%s: The parameter '%s' is unrecognized.", m_module.c_str(), name.c_str());
            configured = false;
        }
    }

    if (configured)
    {
        configured = configuration.configure();
    }

    return configured;
}

void Specification::populate(MXS_MODULE& module) const
{
    MXS_MODULE_PARAM* pModule_param = &module.parameters[0];

    for (const auto& entry : m_params)
    {
        const Param* pParam = entry.second;

        pParam->populate(*pModule_param);
        ++pModule_param;
    }
}

size_t Specification::size() const
{
    return m_params.size();
}

void Specification::insert(Param* pParam)
{
    mxb_assert(m_params.find(pParam->name()) == m_params.end());

    m_params.insert(make_pair(pParam->name(), pParam));
}

void Specification::remove(Param* pParam)
{
    auto it = m_params.find(pParam->name());
    mxb_assert(it != m_params.end());

    m_params.erase(it);
}


/**
 * class Param
 */
Param::Param(Specification* pSpecification,
             const char* zName,
             const char* zDescription,
             Kind kind,
             mxs_module_param_type legacy_type)
    : m_specification(*pSpecification)
    , m_name(zName)
    , m_description(zDescription)
    , m_kind(kind)
    , m_legacy_type(legacy_type)
{
    m_specification.insert(this);
}

Param::~Param()
{
    m_specification.remove(this);
}

const string& Param::name() const
{
    return m_name;
}

const string& Param::description() const
{
    return m_description;
}

std::string Param::documentation() const
{
    std::stringstream ss;

    ss << m_name << " (" << type() << ", ";

    if (is_mandatory())
    {
        ss << "mandatory";
    }
    else
    {
        ss << "optional, default: " << default_to_string();
    }

    ss << "): " << m_description;

    return ss.str();
}

Param::Kind Param::kind() const
{
    return m_kind;
}

bool Param::is_mandatory() const
{
    return m_kind == MANDATORY;
}

bool Param::is_optional() const
{
    return m_kind == OPTIONAL;
}

bool Param::has_default_value() const
{
    return is_optional();
}

void Param::populate(MXS_MODULE_PARAM& param) const
{
    param.type = m_legacy_type;
    param.name = MXS_STRDUP_A(name().c_str());

    if (has_default_value())
    {
        string s = default_to_string().c_str();

        if ((s.length() >= 2) && (s.at(0) == '"') && (s.at(s.length() - 1) == '"'))
        {
            s = s.substr(1, s.length() - 2);
        }

        param.default_value = MXS_STRDUP_A(s.c_str());
    }

    if (is_mandatory())
    {
        param.options |= MXS_MODULE_OPT_REQUIRED;
    }
}


/**
 * class Configuration
 */
Configuration::Configuration(const config::Specification* pSpecification)
    : m_specification(*pSpecification)
{
}

const config::Specification& Configuration::specification() const
{
    return m_specification;
}

Type* Configuration::find_value(const string& name)
{
    auto it = m_values.find(name);

    return it != m_values.end() ? it->second : nullptr;
}

const Type* Configuration::find_value(const string& name) const
{
    return const_cast<Configuration*>(this)->find_value(name);
}

ostream& Configuration::persist(ostream& out) const
{
    for (const auto& entry : m_values)
    {
        Type* pValue = entry.second;
        pValue->persist(out) << "\n";
    }

    return out;
}

void Configuration::insert(Type* pValue)
{
    mxb_assert(m_values.find(pValue->parameter().name()) == m_values.end());

    m_values.insert(make_pair(pValue->parameter().name(), pValue));
}

void Configuration::remove(Type* pValue)
{
    auto it = m_values.find(pValue->parameter().name());

    mxb_assert(it != m_values.end());
    m_values.erase(it);
}

bool Configuration::configure()
{
    return true;
}

size_t Configuration::size() const
{
    return m_values.size();
}

/**
 * class Type
 */
Type::Type(Configuration* pConfiguration, const config::Param* pParam)
    : m_configuration(*pConfiguration)
    , m_param(*pParam)
{
    m_configuration.insert(this);
}

Type::~Type()
{
    m_configuration.remove(this);
}

const config::Param& Type::parameter() const
{
    return m_param;
}

ostream& Type::persist(ostream& out) const
{
    out << m_param.name() << "=" << to_string();
    return out;
}

bool Type::set(const string& value_as_string)
{
    return m_param.set(*this, value_as_string);
}

/**
 * ParamBool
 */
std::string ParamBool::type() const
{
    return "boolean";
}

std::string ParamBool::default_to_string() const
{
    return to_string(m_default_value);
}

bool ParamBool::validate(const std::string& value_as_string, std::string* pMessage) const
{
    value_type value;
    return from_string(value_as_string, &value, pMessage);
}

bool ParamBool::set(Type& value, const std::string& value_as_string) const
{
    mxb_assert(&value.parameter() == this);

    Bool& bool_value = static_cast<Bool&>(value);

    value_type x;
    bool valid = from_string(value_as_string, &x);

    if (valid)
    {
        bool_value.set(x);
    }

    return valid;
}

bool ParamBool::from_string(const string& value_as_string, value_type* pValue, string* pMessage) const
{
    int rv = config_truth_value(value_as_string.c_str());

    if (rv == 1)
    {
        *pValue = true;
    }
    else if (rv == 0)
    {
        *pValue = false;
    }
    else if (pMessage)
    {
        mxb_assert(rv == -1);

        *pMessage = "Invalid boolean: ";
        *pMessage += value_as_string;
    }

    return rv != -1;
}

string ParamBool::to_string(value_type value) const
{
    return value ? "true" : "false";
}

/**
 * ParamCount
 */
std::string ParamCount::type() const
{
    return "count";
}

std::string ParamCount::default_to_string() const
{
    return to_string(m_default_value);
}

bool ParamCount::validate(const std::string& value_as_string, std::string* pMessage) const
{
    value_type value;
    return from_string(value_as_string, &value, pMessage);
}

bool ParamCount::set(Type& value, const std::string& value_as_string) const
{
    mxb_assert(&value.parameter() == this);

    Count& count_value = static_cast<Count&>(value);

    value_type x;
    bool valid = from_string(value_as_string, &x);

    if (valid)
    {
        count_value.set(x);
    }

    return valid;
}

bool ParamCount::from_string(const std::string& value_as_string,
                             value_type* pValue,
                             std::string* pMessage) const
{
    const char* zValue = value_as_string.c_str();
    char* zEnd;
    long l = strtol(zValue, &zEnd, 10);
    bool valid = (l >= 0 && zEnd != zValue && *zEnd == 0);

    if (valid)
    {
        *pValue = l;
    }
    else if (pMessage)
    {
        *pMessage = "Invalid count: ";
        *pMessage += value_as_string;
    }

    return valid;
}

std::string ParamCount::to_string(value_type value) const
{
    return std::to_string(value);
}

/**
 * ParamPath
 */
std::string ParamPath::type() const
{
    return "path";
}

std::string ParamPath::default_to_string() const
{
    return to_string(m_default_value);
}

bool ParamPath::validate(const std::string& value_as_string, std::string* pMessage) const
{
    value_type value;
    return from_string(value_as_string, &value, pMessage);
}

bool ParamPath::set(Type& value, const std::string& value_as_string) const
{
    mxb_assert(&value.parameter() == this);

    Path& path_value = static_cast<Path&>(value);

    value_type x;
    bool valid = from_string(value_as_string, &x);

    if (valid)
    {
        path_value.set(x);
    }

    return valid;
}

bool ParamPath::from_string(const std::string& value_as_string,
                            value_type* pValue,
                            std::string* pMessage) const
{
    MXS_MODULE_PARAM param {};
    param.options = m_options;

    bool valid = check_path_parameter(&param, value_as_string.c_str());

    if (valid)
    {
        *pValue = value_as_string;
    }
    else if (pMessage)
    {
        *pMessage = "Invalid path (does not exist, required permissions are not granted, ";
        *pMessage += "or cannot be created): ";
        *pMessage += value_as_string;
    }

    return valid;
}

std::string ParamPath::to_string(const value_type& value) const
{
    return value;
}

void ParamPath::populate(MXS_MODULE_PARAM& param) const
{
    Param::populate(param);

    param.options |= m_options;
}

/**
 * ParamSize
 */
std::string ParamSize::type() const
{
    return "size";
}

std::string ParamSize::default_to_string() const
{
    return to_string(m_default_value);
}

bool ParamSize::validate(const std::string& value_as_string, std::string* pMessage) const
{
    value_type value;
    return from_string(value_as_string, &value, pMessage);
}

bool ParamSize::set(Type& value, const std::string& value_as_string) const
{
    mxb_assert(&value.parameter() == this);

    Size& size_value = static_cast<Size&>(value);

    value_type x;
    bool valid = from_string(value_as_string, &x);

    if (valid)
    {
        size_value.set(x);
    }

    return valid;
}

bool ParamSize::from_string(const std::string& value_as_string,
                            value_type* pValue,
                            std::string* pMessage) const
{
    bool valid = get_suffixed_size(value_as_string.c_str(), pValue);

    if (!valid && pMessage)
    {
        *pMessage = "Invalid size: ";
        *pMessage += value_as_string;
    }

    return valid;
}

std::string ParamSize::to_string(value_type value) const
{
    // TODO: Use largest possible unit.
    return std::to_string(value);
}

/**
 * ParamString
 */
std::string ParamString::type() const
{
    return "string";
}

std::string ParamString::default_to_string() const
{
    return to_string(m_default_value);
}

bool ParamString::validate(const std::string& value_as_string, std::string* pMessage) const
{
    value_type value;
    return from_string(value_as_string, &value, pMessage);
}

bool ParamString::set(Type& value, const std::string& value_as_string) const
{
    mxb_assert(&value.parameter() == this);

    String& string_value = static_cast<String&>(value);

    value_type x;
    bool valid = from_string(value_as_string, &x);

    if (valid)
    {
        string_value.set(x);
    }

    return valid;
}

bool ParamString::from_string(const std::string& value_as_string,
                              value_type* pValue,
                              std::string* pMessage) const
{
    bool valid = true;

    char b = value_as_string.empty() ? 0 : value_as_string.front();
    char e = value_as_string.empty() ? 0 : value_as_string.back();

    if (b != '"' && b != '\'')
    {
        if (pMessage)
        {
            *pMessage = "A string value should be enclosed in quotes: ";
            *pMessage += value_as_string;
        }
    }

    string s = value_as_string;

    if (b == '"' || b == '\'')
    {
        valid = (b == e);

        if (valid)
        {
            s = s.substr(1, s.length() - 2);
        }
        else if (pMessage)
        {
            *pMessage = "A quoted string must end with the same quote: ";
            *pMessage += value_as_string;
        }
    }

    if (valid)
    {
        *pValue = s;
    }

    return valid;
}

std::string ParamString::to_string(value_type value) const
{
    stringstream ss;
    ss << "\"" << value << "\"";
    return ss.str();
}

}