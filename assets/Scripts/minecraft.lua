local W = 28
local blocks = {}
local objs = {}
local seed = 777

local function hash(x,z)
    local h = math.sin(x*127.1 + z*311.7 + seed*0.613) * 43758.5453
    return h - math.floor(h)
end
local function sm(a,b,t) return a+(b-a)*(t*t*(3-2*t)) end
local function vnoise(x,z)
    local xi, zi = math.floor(x), math.floor(z)
    local fx, fz = x-xi, z-zi
    return sm(sm(hash(xi,zi),hash(xi+1,zi),fx), sm(hash(xi,zi+1),hash(xi+1,zi+1),fx), fz)
end
local function fbm(x,z)
    local s, amp, f, tot = 0, 1, 1, 0
    for i=1,4 do s=s+vnoise(x*f,z*f)*amp; tot=tot+amp; amp=amp*0.5; f=f*2 end
    return s/tot
end

local yaw, pitch = 0.0, -12.0
local px, py, pz = 14, 20, 14
local vy = 0
local grounded = false
local prevL, prevR = false, false

local function colorFor(h)
    if h < 5 then return 0.85,0.80,0






.55 end
    if h < 9 then return 0.35,0.62,0.25 end
    return 0.50,0.50,0.52
end

local function makeBlock(x,z,h)
    local i = Object.create("cube","block")
    Object.setPosition(i, x+0.5, h+0.5, z+0.5)
    local r,g,b = colorFor(h)
    Object.setColor(i,r,g,b)
    return i
end

function onStart()
    Object.setPosition(selfIndex, 0,-100,0)















    for x=0,W-1 do
        blocks[x]={}
        objs[x]={}
        for z=0,W-1 do
            local h = math.floor(3 + fbm(x*0.09,z*0.09)*9)
            blocks[x][z]=h
            objs[x][z]=makeBlock(x,z,h)
        end
    end
 








   py = blocks[math.floor(px)][math.floor(pz)] + 2
    Input.captureMouse(true)
    print("Minecraft world ready")
end

local function rayBlock()
    local cp = math.cos(math.rad(pitch))
 






   local fx = math.cos(math.rad(yaw))*cp
    local fy = math.sin(math.rad(pitch))
    local fz = math.sin(math.rad(yaw))*cp
    local rx, ry, rz = px, py+1.6, pz
    for s=1,60 do
 




       rx = rx+fx*0.1
        ry = ry+fy*0.1
        rz = rz+fz*0.1
        if ry < 0 then return nil end
 



       local x, z = math.floor(rx), math.floor(rz)
        if x<0 or z<0 or x>=W or z>=W then return nil end
        local h = blocks[x][z]
        if ry >= h and ry <= h+1 then return x, z end




    end
    return nil
end




local function breakBlock(x,z)
    local h = blocks[x][z]
 


   if h <= 0 then return end
    Object.destroy(objs[x][z])
    blocks[x][z] = h-1
 


   objs[x][z] = makeBlock(x,z,h-1)
end

l


ocal function placeBlock(x,z)
    local h = blocks[x][z]
    if h+1 > 30 then return end
 


   blocks[x][z] = h+1
    objs[x][z] = makeBlock(x,z,h+1)
end




function onUpdate(dt)
    local mdx, mdy = Input.getMouseDelta()



    yaw = yaw - mdx*0.15
    pitch = pitch - mdy*0.1
 

   if pitch>89 then pitch=89 end
    if pitch<-89 then pitch=-89 end
 

   local sp = 6
    local fx = math.cos(math.rad(yaw))
 

   local fz = math.sin(math.rad(yaw))
    local mx, mz = 0,0
 

   if Input.isKeyDown("W") then mx=mx+fx; mz=mz+fz end
    if Input.isKeyDown("S") then mx=mx-fx; mz=mz-fz end
 

   if Input.isKeyDown("D") then mx=mx-fz; mz=mz+fx end
    if Input.isKeyDown("A") then mx=mx+fz; mz=mz-fx end
 

   local len = math.sqrt(mx*mx+mz*mz)
    if len>0 then mx=mx/len*sp; mz=mz/len*sp end
 

   px = px + mx*dt
    pz = pz + mz*dt
 

   if px<0 then px=0.01 end
    if pz<0 then pz=0.01 end
 

   if px>=W then px=W-0.01 end
    if pz>=W then pz=W-0.01 end
 

   if Input.isKeyDown("Space") and grounded then vy=8; grounded=false end
    vy = vy - 24*dt
 

   py = py + vy*dt
    local ground = blocks[math.floor(px)][math.floor(pz)] + 1
 

   if py <= ground then py=ground; vy=0; grounded=true else grounded=false end
    local cp = math.cos(math.rad(pitch))
 

   Camera.setPos(px, py+1.6, pz)
    Camera.lookAt(px+math.cos(math.rad(yaw))*cp, py+1.6+math.sin(math.rad(pitch)), pz+math.s
in(math.rad(yaw))*cp)
    local l = Input.isKeyDown("LMB")
 

   local r = Input.isKeyDown("RMB")
    if l and not prevL then
 

       local x,z = rayBlock()
        if x then breakBlock(x,z) end


    end
 
   if r and not prevR then
        local x,
z = rayBlock()
        if x then placeBlock(x,z) end


    end
    prevL, prevR = l
, r
end



