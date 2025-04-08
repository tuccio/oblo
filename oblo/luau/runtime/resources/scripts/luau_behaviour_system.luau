local m = {
    entities = {}
}

function m:add(e : integer, script : () -> table)
    self.entities[#self.entities + 1] = {
        entity = e,
        script = script
    }
end

function m:update()
    local entities = self.entities;
    local i = 1

    oblo.log.debug(`Running update on {#entities} entities`)

    while i <= #entities do
        local v = entities[i]

        -- Check if the entity was removed
        if not oblo.ecs.is_alive(v.entity) then
            local count = #entities
            entities[i] = entities[count]
            table.remove(entities, count)

            continue
        end

        if v.script then
            v.userdata = v.script()
            v.script = nil
        end

        if v.userdata then
            v.userdata:update()
        end

        i = i + 1
    end
end

return m
