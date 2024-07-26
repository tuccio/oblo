#pragma once



namespace oblo
{
    struct sandbox_state;

    class scene_importer
    {
    public:
        bool import(sandbox_state& state, string_view filename);
    };
}