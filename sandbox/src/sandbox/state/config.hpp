#pragma once

namespace oblo
{
    struct sandbox_state;

    bool config_parse(const char* filename, sandbox_state& state);
    bool config_write(const char* filename, const sandbox_state& state);
}