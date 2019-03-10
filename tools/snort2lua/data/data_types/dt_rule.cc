//--------------------------------------------------------------------------
// Copyright (C) 2014-2019 Cisco and/or its affiliates. All rights reserved.
//
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License Version 2 as published
// by the Free Software Foundation.  You may not use, modify or distribute
// this program under any other version of the GNU General Public License.
//
// This program is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
//--------------------------------------------------------------------------
// dt_rule.cc author Josh Rosenbaum <jrosenba@cisco.com>

#include "data/data_types/dt_rule.h"
#include "data/dt_data.h"  // included for print mode
#include "helpers/s2l_util.h"
#include "data/data_types/dt_rule_option.h"

Rule::Rule() :  num_hdr_data(0),
    is_bad_rule(false),
    is_comment(false), old_http_rule(false)
{
}

Rule::~Rule()
{
    for (RuleOption* ro : options)
        delete ro;
}

bool Rule::add_hdr_data(const std::string& data)
{
    if (num_hdr_data < hdr_data.size())
    {
        hdr_data[num_hdr_data] = data;
        num_hdr_data++;
        return true;
    }
    else
    {
        is_bad_rule = true;
        return false;
    }
}

void Rule::set_rule_old_action(const std::string& action)
{
    old_action = action;
}

std::string& Rule::get_rule_old_action()
{
    return old_action;
}

void Rule::update_rule_action(const std::string& new_type)
{ hdr_data[0] = new_type; }

void Rule::bad_rule()
{ is_bad_rule = true; }

void Rule::add_comment(const std::string& new_comment)
{ comments.push_back("# " + new_comment); }

void Rule::make_comment()
{ is_comment = true; }

void Rule::set_old_http_rule()
{ old_http_rule = true; }

void Rule::add_option(const std::string& keyword)
{
    RuleOption* r = new RuleOption(keyword);
    options.push_back(r);
}

void Rule::add_option(const std::string& keyword, const std::string& data)
{
    RuleOption* r = new RuleOption(keyword, data);
    options.push_back(r);
}

std::string Rule::get_option(const std::string& keyword)
{
    for (auto option : options)
    {
        if (option->get_name() == keyword)
            return option->get_value();
    }
    return std::string();
}

void Rule::update_option(const std::string& keyword, std::string& val)
{
    for (auto option : options)
    {
        if (option->get_name() == keyword)
        {
            option->update_value(val);
            break;
        }
    }
}

void Rule::add_suboption(const std::string& keyword)
{ options.back()->add_suboption(keyword); }

void Rule::add_suboption(const std::string& keyword, const std::string& val)
{ options.back()->add_suboption(keyword, val); }

void Rule::set_curr_options_buffer(const std::string& new_buffer, bool add_option)
{
    if (new_buffer == "pkt_data")
    {
        if (sticky_buffer.empty())
        {
            sticky_buffer = "pkt_data";
            return;
        }

        if (sticky_buffer == "pkt_data")
        {
            return;
        }
    }

    RuleOption* new_opt = new RuleOption(new_buffer);
    if ( add_option )
        options.push_back(new_opt);
    else
        options.insert(options.end() - 1, new_opt);

    sticky_buffer = new_buffer;
}

std::ostream& operator<<(std::ostream& out, const Rule& rule)
{
    bool first_line = true;

    // don't print comment and tag in quiet mode
    if (!DataApi::is_quiet_mode())
    {
        for (const std::string& s : rule.comments)
            out << s << "\n";
    }

    if (rule.is_bad_rule || rule.is_comment)
        out << "# ";

    for (std::size_t i = 0; i < rule.num_hdr_data; i++)
    {
        if (first_line)
            first_line = false;
        else
            out << " ";

        std::string tmp = rule.hdr_data[i];
        out << util::sanitize_lua_string(tmp);
    }

    if (!rule.options.empty())
    {
        out << " (";
        first_line = true;

        for (auto* r : rule.options)
        {
            if (first_line)
                first_line = false;
            else
                out << ";";
            out << " " << (*r);
        }

        out << "; )";
    }

    return out;
}

void Rule::resolve_pcre_buffer_options()
{
    std::string curr_sticky_buffer = "";
    const std::string service = get_option("service");
    const bool service_sip = (service.find("sip") != std::string::npos);
    const bool no_service_http = (service.find("http") == std::string::npos);
    std::string new_buffer;
    std::vector<RuleOption*>::iterator iter = options.begin();
    std::vector<RuleOption*>::iterator next_opt_iter;

    while (iter != options.end())
    {
        std::string name = (*iter)->get_name();

        if (name == "pcre_P_option_body" ||
            name == "pcre_P_option_body_rel" ||
            name == "pcre_H_option_header" ||
            name == "pcre_H_option_header_rel")
        {
            delete(*iter);
            iter = options.erase(iter);

            if (service_sip)
            {
                if (name == "pcre_P_option_body" ||
                    name == "pcre_P_option_body_rel")
                {
                    new_buffer = "sip_body";
                }
                else
                {
                    new_buffer = "sip_header";
                }
            }
            else
            {
                if (name == "pcre_P_option_body" ||
                    name == "pcre_P_option_body_rel")
                {
                    if (no_service_http)
                    {
                        add_comment("pcre P option converted to http_client_body by default");
                    }
                    new_buffer = "http_client_body";
                }
                else
                {
                    if (no_service_http)
                    {
                        add_comment("pcre H option converted to http_header by default");
                    }
                    new_buffer = "http_header";
                }
            }

            /* Add sticky buffer option if not equal to current,
             * or if the pcre option is not relative */
            if (curr_sticky_buffer != new_buffer ||
                (name != "pcre_P_option_body_rel" &&
                name != "pcre_H_option_header_rel"))
            {
                curr_sticky_buffer = new_buffer;
                RuleOption* new_opt = new RuleOption(new_buffer);
                options.insert(iter, new_opt);
                ++iter;
            }
        }
        else if (name == "pkt_data" ||
            name == "file_data" ||
            name == "dce_stub_data" ||
            name == "dnp3_data" ||
            name == "modbus_data" ||
            name == "sip_header" ||
            name == "sip_body")
        {
            curr_sticky_buffer = name;
            ++iter;
        }
        else if (name == "http_header" ||
            name == "http_client_body" ||
            name == "http_cookie" ||
            name == "http_method" ||
            name == "http_raw_cookie" ||
            name == "http_raw_header" ||
            name == "http_raw_uri" ||
            name == "http_stat_code" ||
            name == "http_stat_msg" ||
            name == "http_uri")
        {
            if (curr_sticky_buffer == name)
            {
                next_opt_iter = std::next(iter, 1);
                if (next_opt_iter != options.end())
                {
                    if ((*next_opt_iter)->is_relative_content())
                    {
                        delete(*iter);
                        iter = options.erase(iter);
                    }
                    else
                    {
                        ++iter;
                    }
                }
                else
                {
                    ++iter;
                }
            }
            else
            {
                curr_sticky_buffer = name;
                ++iter;
            }
        }
        else
        {
            ++iter;
        }
    }
}

