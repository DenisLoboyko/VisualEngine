-- Test.lua
-- Attach this to Cube. Exercises Object.getPosition / setPosition / setScale
-- every frame through the Object.* table API to confirm GameLua no longer
-- reads a stale lua_upvalueindex(1) and crashes lua55.dll.

local t = 0

function onStart()
    print("Test.lua started, selfIndex=" .. tostring(selfIndex))
end

function onUpdate(dt)
    t = t + dt

    local x, y, z = Object.getPosition(selfIndex)

    x = math.sin(t) * 2.0
    y = 0.5 + math.sin(t * 2.0) * 0.3
    z = math.cos(t) * 2.0
    Object.setPosition(selfIndex, x, y, z)

    local s = 1.0 + math.sin(t * 3.0) * 0.2
    Object.setScale(selfIndex, s, s, s)

    if math.floor(t) % 5 == 0 and math.floor(t * 10) % 50 == 0 then
        print(string.format("Test.lua: pos=(%.2f, %.2f, %.2f) scale=%.2f", x, y, z, s))
    end
end