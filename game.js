// game.js (complete)
let gameInstance = null;
function startGame(playerName) { gameInstance = new Game(playerName); }

class Game {
    constructor(playerName) {
        this.canvas = document.getElementById('game-canvas');
        this.renderer = new THREE.WebGLRenderer({ canvas: this.canvas, antialias: true });
        this.renderer.setSize(window.innerWidth, window.innerHeight);
        this.renderer.shadowMap.enabled = true;
        this.scene = new THREE.Scene();
        this.scene.background = new THREE.Color(0x87CEEB);
        this.scene.fog = new THREE.Fog(0x87CEEB, 80, 350);
        this.camera = new THREE.PerspectiveCamera(70, innerWidth/innerHeight, 0.5, 400);
        this.initWorld();
        this.player = new Player(0, 0, 0, true);
        this.player.name = playerName;
        this.player.mesh = this.createPlayerMesh(0xffaa00);
        this.scene.add(this.player.mesh);
        this.enemies = [];
        this.spawnEnemies(12);
        this.stormRadius = 200;
        this.stormTarget = 20;
        this.stormTimer = 40;
        this.raycaster = new THREE.Raycaster();
        this.clock = new THREE.Clock();
        this.input = new InputController(this.canvas);
        UI.init();
        this.gameLoop();
        window.addEventListener('resize', () => {
            this.camera.aspect = innerWidth/innerHeight;
            this.camera.updateProjectionMatrix();
            this.renderer.setSize(innerWidth, innerHeight);
        });
    }

    createPlayerMesh(color) {
        const group = new THREE.Group();
        const body = new THREE.Mesh(new THREE.BoxGeometry(0.7,1.6,0.4), new THREE.MeshLambertian({color}));
        body.position.y = 1;
        group.add(body);
        const head = new THREE.Mesh(new THREE.SphereGeometry(0.25), new THREE.MeshLambertian({color:0xffcc66}));
        head.position.y = 1.75;
        group.add(head);
        return group;
    }

    initWorld() {
        const ground = new THREE.Mesh(new THREE.PlaneGeometry(MAP_SIZE, MAP_SIZE),
            new THREE.MeshLambertian({color:0x3a6b1e}));
        ground.rotation.x = -Math.PI/2; ground.position.y = -0.1; ground.receiveShadow = true;
        this.scene.add(ground);
        this.scene.add(new THREE.AmbientLight(0x404066));
        const sun = new THREE.DirectionalLight(0xffffff, 0.9);
        sun.position.set(100,200,50); sun.castShadow = true;
        this.scene.add(sun);
        for (let i=0; i<80; i++) {
            const x = (Math.random()-0.5)*MAP_SIZE, z = (Math.random()-0.5)*MAP_SIZE;
            const trunk = new THREE.Mesh(new THREE.CylinderGeometry(0.2,0.3,3),
                new THREE.MeshLambertian({color:0x6b4c3a}));
            trunk.position.set(x,1.5,z); trunk.castShadow = true; this.scene.add(trunk);
            const leaves = new THREE.Mesh(new THREE.ConeGeometry(1,2,8),
                new THREE.MeshLambertian({color:0x3a7a2a}));
            leaves.position.set(x,3.2,z); leaves.castShadow = true; this.scene.add(leaves);
        }
        BuildingSystem.init();
        this.structureMeshes = new THREE.Group(); this.scene.add(this.structureMeshes);
        // storm + chests
        this.stormMesh = new THREE.Mesh(new THREE.CylinderGeometry(200,200,400,64),
            new THREE.MeshBasicMaterial({color:0x9944ff, transparent:true, opacity:0.2, side:THREE.DoubleSide}));
        this.stormMesh.position.y = 200; this.scene.add(this.stormMesh);
        this.chests = [];
        const chestGeo = new THREE.BoxGeometry(0.8,0.6,0.8);
        for (let i=0; i<15; i++) {
            const chest = new THREE.Mesh(chestGeo, new THREE.MeshLambertian({color:0xd4a840}));
            chest.position.set((Math.random()-0.5)*MAP_SIZE, 0.3, (Math.random()-0.5)*MAP_SIZE);
            chest.castShadow = true; this.scene.add(chest);
            this.chests.push({mesh: chest, opened: false});
        }
    }

    spawnEnemies(n) {
        for (let i=0; i<n; i++) {
            const e = new Enemy((Math.random()-0.5)*MAP_SIZE, (Math.random()-0.5)*MAP_SIZE);
            e.mesh = this.createPlayerMesh(0xdd3333);
            this.scene.add(e.mesh);
            this.enemies.push(e);
        }
    }

    handleInput(dt) {
        const p = this.player;
        if (!p.alive) return;
        const inp = this.input;
        p.yaw += inp.mouseDx * 0.002;
        p.pitch -= inp.mouseDy * 0.002;
        p.pitch = clamp(p.pitch, -1.2, 0.5);
        inp.mouseDx = inp.mouseDy = 0;
        let moveX = 0, moveZ = 0;
        if (inp.forward) { moveX += Math.sin(p.yaw); moveZ += Math.cos(p.yaw); }
        if (inp.backward) { moveX -= Math.sin(p.yaw); moveZ -= Math.cos(p.yaw); }
        if (inp.left) { moveX -= Math.cos(p.yaw); moveZ += Math.sin(p.yaw); }
        if (inp.right) { moveX += Math.cos(p.yaw); moveZ -= Math.sin(p.yaw); }
        const len = Math.sqrt(moveX*moveX+moveZ*moveZ);
        if (len > 0) { moveX /= len; moveZ /= len; }
        const speed = inp.sprint ? 18 : 12;
        p.velocity.x = moveX * speed;
        p.velocity.z = moveZ * speed;
        if (!p.onGround) p.velocity.y -= GRAVITY * dt;
        if (inp.jump && p.onGround) { p.velocity.y = 14; p.onGround = false; }
        p.position.x += p.velocity.x * dt;
        p.position.y += p.velocity.y * dt;
        p.position.z += p.velocity.z * dt;
        if (p.position.y <= 0) { p.position.y = 0; p.velocity.y = 0; p.onGround = true; }
        p.position.x = clamp(p.position.x, -HALF_MAP+1, HALF_MAP-1);
        p.position.z = clamp(p.position.z, -HALF_MAP+1, HALF_MAP-1);
        // building
        if (inp.buildToggle) { p.buildMode = !p.buildMode; inp.buildToggle = false; }
        if (p.buildMode) {
            if (inp.scrollUp) { p.buildPiece = ['wall','floor','ramp','roof'][(p.buildPiece+1)%4]; }
            if (inp.scrollDown) { p.buildPiece = ['wall','floor','ramp','roof'][(p.buildPiece+3)%4]; }
            if (inp.fire) {
                const g = worldToGrid(p.position.x + Math.sin(p.yaw)*3, p.position.z + Math.cos(p.yaw)*3);
                if (BuildingSystem.placeStructure(g.ix, g.iz, p.buildPiece, p.materials)) {
                    this.refreshStructures();
                }
                inp.fire = false;
            }
        } else {
            if (inp.fire) p.shoot(this);
        }
        // weapon switch
        if (inp.switchWeapon !== -1) { p.activeSlot = inp.switchWeapon; inp.switchWeapon = -1; }
        if (inp.reload) { p.startReload(); inp.reload = false; }
        p.updateReload(dt);
    }

    refreshStructures() {
        while(this.structureMeshes.children.length) this.structureMeshes.remove(this.structureMeshes.children[0]);
        const wallGeo = new THREE.BoxGeometry(2,3,2);
        const mat = new THREE.MeshLambertian({color:0x8B7355});
        for (let iz=0; iz<GRID_DIM; iz++) for (let ix=0; ix<GRID_DIM; ix++) {
            if (BuildingSystem.getTile(ix,iz)===1) {
                const m = new THREE.Mesh(wallGeo, mat);
                m.position.set((ix+0.5)*GRID_RES-HALF_MAP, 1.5, (iz+0.5)*GRID_RES-HALF_MAP);
                m.castShadow = true;
                this.structureMeshes.add(m);
            }
        }
    }

    updateStorm(dt) {
        if (this.stormTimer > 0) {
            this.stormTimer -= dt;
            if (this.stormTimer <= 0) this.stormTarget = Math.max(15, this.stormRadius - 60);
        }
        if (this.stormRadius > this.stormTarget) {
            this.stormRadius -= 2 * dt;
            if (this.stormRadius < this.stormTarget) this.stormRadius = this.stormTarget;
        }
        // damage outside
        [this.player, ...this.enemies].forEach(e => {
            if (!e.alive) return;
            const d = Math.sqrt(e.position.x*e.position.x + e.position.z*e.position.z);
            if (d > this.stormRadius) e.damage(5*dt);
        });
    }

    alivePlayers() {
        let c = this.player.alive ? 1 : 0;
        this.enemies.forEach(e => { if (e.alive) c++; });
        return c;
    }

    getClosestEnemy(me) {
        let best = null, bestD = Infinity;
        const pool = me === this.player ? this.enemies : [this.player, ...this.enemies.filter(e=>e!==me)];
        pool.forEach(e => { if (e.alive) { const d = dist3(me.position, e.position); if (d<bestD) { bestD=d; best=e; } } });
        return best;
    }

    raycastShoot(shooter, weapon) {
        const origin = shooter.position.clone();
        const dir = new THREE.Vector3(Math.sin(shooter.yaw), Math.sin(shooter.pitch), Math.cos(shooter.yaw)).normalize();
        if (weapon.spread) {
            dir.x += (Math.random()-0.5)*weapon.spread;
            dir.y += (Math.random()-0.5)*weapon.spread;
            dir.z += (Math.random()-0.5)*weapon.spread;
            dir.normalize();
        }
        this.raycaster.set(origin, dir);
        const targets = shooter === this.player ? this.enemies : [this.player];
        for (let t of targets) {
            if (!t.alive) continue;
            const box = new THREE.Box3().setFromObject(t.mesh);
            if (this.raycaster.ray.intersectBox(box, new THREE.Vector3())) {
                t.damage(weapon.dmg);
                if (shooter === this.player) UI.notify('HIT ' + Math.round(weapon.dmg));
                if (!t.alive) {
                    shooter.kills++;
                    UI.addKill(`${shooter.name} eliminated ${t.name}`);
                }
                return;
            }
        }
    }

    gameLoop() {
        requestAnimationFrame(() => this.gameLoop());
        const dt = Math.min(this.clock.getDelta(), 0.1);
        this.handleInput(dt);
        this.enemies.forEach(e => { if (e.alive) e.update(dt, this); });
        this.updateStorm(dt);
        this.player.mesh.position.copy(this.player.position);
        this.player.mesh.visible = this.player.alive;
        this.enemies.forEach(e => { e.mesh.position.copy(e.position); e.mesh.rotation.y = e.yaw; e.mesh.visible = e.alive; });
        if (Math.random()<0.05) this.refreshStructures();
        this.stormMesh.scale.set(this.stormRadius/200, 1, this.stormRadius/200);
        const p = this.player.position, yaw = this.player.yaw, pitch = this.player.pitch;
        const camDist = 4.5;
        this.camera.position.lerp(new THREE.Vector3(
            p.x - Math.sin(yaw)*camDist*Math.cos(pitch),
            p.y + 2.5 + Math.sin(pitch)*camDist,
            p.z - Math.cos(yaw)*camDist*Math.cos(pitch)
        ), 0.15);
        this.camera.lookAt(p.x, p.y+1.2, p.z);
        this.renderer.render(this.scene, this.camera);
        UI.update(this.player, this);
        if (!this.player.alive) UI.notify('ELIMINATED');
        else if (this.alivePlayers()===1) UI.notify('VICTORY ROYALE');
    }
}

// Input controller class
class InputController {
    constructor(canvas) {
        this.forward = this.backward = this.left = this.right = false;
        this.jump = false; this.sprint = false;
        this.fire = false; this.buildToggle = false; this.reload = false;
        this.mouseDx = 0; this.mouseDy = 0;
        this.switchWeapon = -1;
        this.scrollUp = false; this.scrollDown = false;
        document.addEventListener('keydown', e => this.onKey(e, true));
        document.addEventListener('keyup', e => this.onKey(e, false));
        document.addEventListener('mousemove', e => { if (document.pointerLockElement===canvas) { this.mouseDx += e.movementX; this.mouseDy += e.movementY; } });
        document.addEventListener('mousedown', e => { if (e.button===0) this.fire = true; });
        document.addEventListener('mouseup', e => { if (e.button===0) this.fire = false; });
        document.addEventListener('wheel', e => { this.scrollUp = e.deltaY<0; this.scrollDown = e.deltaY>0; });
        canvas.addEventListener('click', () => canvas.requestPointerLock());
    }
    onKey(e, down) {
        switch(e.code) {
            case 'KeyW': this.forward = down; break;
            case 'KeyS': this.backward = down; break;
            case 'KeyA': this.left = down; break;
            case 'KeyD': this.right = down; break;
            case 'Space': this.jump = down; e.preventDefault(); break;
            case 'ShiftLeft': this.sprint = down; break;
            case 'KeyQ': if (down) this.buildToggle = true; break;
            case 'KeyR': if (down) this.reload = true; break;
            case 'Digit1': if (down) this.switchWeapon = 0; break;
            case 'Digit2': if (down) this.switchWeapon = 1; break;
            case 'Digit3': if (down) this.switchWeapon = 2; break;
            case 'Digit4': if (down) this.switchWeapon = 3; break;
            case 'Digit5': if (down) this.switchWeapon = 4; break;
        }
    }
}
