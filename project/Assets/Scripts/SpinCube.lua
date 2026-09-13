local t = 0

function onStart()
    print("SpinCube started!")
end

function onUpdate(dt)
    t = t + dt
    Scene.SetPosition("Cube", math.sin(t) * 2, 0.5 + math.sin(t * 2) * 0.3, math.cos(t) * 2)
end
