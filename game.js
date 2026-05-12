// game.js
let gameInstance = null;
function startGame(playerName) {
    gameInstance = new Game(playerName);
}

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

    initWorld() {
        // ground
        const groundGeo = new THREE.PlaneGeometry(MAP_SIZE, MAP_SIZE);
        const groundMat = new THREE.MeshLambertian({ color: 0x3a6b1e });
        const ground = new THREE.Mesh(groundGeo, groundMat);
        ground.rotation.x = -Math.PI/2;
        ground.position.y = -0.1;
        ground.receiveShadow = true;
        this.scene.add(ground);
        new THREE.DirectionalLight(0xffffff, 0.9).position.set(100,200,50);
        this.scene.add(new THREE.AmbientLight(0x404066));
        // trees
        for (let i=0; i<80; i++) {
            const x = (Math.random()-0.5)*MAP_SIZE;
            const z = (Math.random()-0.5)*MAP_SIZE;
            const trunk = new THREE.Mesh(new THREE.CylinderGeometry(0.2,0.3,3), new THREE.MeshLambertian({color:0x6b4c3a}));
            trunk.position.set(x,1.5,z);
            trunk.castShadow = trunk.receiveShadow = true;
            this.scene.add(trunk);
            const leaves = new THREE.Mesh(new THREE.ConeGeometry(1,2,8), new THREE.MeshLambertian({color:0x3a7a2a}));
            leaves.position.set(x,3.2,z);
            leaves.castShadow = true;
            this.scene.add(leaves);
        }
        BuildingSystem.init();
        // pre‑placed walls
        for (let i=0; i<10; i++) {
            const ix = Math.floor(Math.random()*GRID_DIM);
            const iz = Math.floor(Math.random()*GRID_DIM);
            BuildingSystem.setTile(ix, iz, 1);
        }
        this.structureMeshes = new THREE.Group();
        this.scene.add(this.structureMeshes);
        this.refreshStructures();
        // storm cylinder
        const stormGeo = new THREE.CylinderGeometry(200,200,400,64);
        const stormMat = new THREE.MeshBasicMaterial({color:0x9944ff, transparent:true, opacity:0.2, side:THREE.DoubleSide});
        this.stormMesh = new THREE.Mesh(stormGeo, stormMat);
        this.stormMesh.position.y = 200;
        this.scene.add(this.stormMesh);
        // chests
        this.chests = [];
        const chestGeo = new THREE.BoxGeometry(0.8,0.6,0.8);
        for (let i=0; i<15; i++) {
            const chest = new THREE.Mesh(chestGeo, new THREE.MeshLambertian({color:0xd4a840}));
            chest.position.set((Math.random()-0.5)*MAP_SIZE, 0.3, (Math.random()-0.5)*MAP_SIZE);
            chest.castShadow = chest.receiveShadow = true;
            this.scene.add(chest);
            this.chests.push({mesh: chest, opened: false});
        }
    }

    spawnEnemies(n) {
        for (let i=0; i<n; i++) {
            const e = new Enemy((Math.random()-0.5)*MAP_SIZE, (Math.random()-0.5)*MAP_SIZE);
            e.addWeapon(2); // give a shotgun too
            this.enemies.push(e);
            // 3D mesh
            e.mesh = new THREE.Mesh(new THREE.BoxGeometry(0.7,1.8,0.7), new THREE.MeshLambertian({color:0xdd3333}));
            e.mesh.castShadow = true;
            this.scene.add(e.mesh);
        }
    }

    refreshStructures() {
        while(this.structureMeshes.children.length) this.structureMeshes.remove(this.structureMeshes.children[0]);
        const wallGeo = new THREE.BoxGeometry(2,3,2);
        const matWall = new THREE.MeshLambertian({color:0x8B7355});
        for (let iz=0; iz<GRID_DIM; iz++) {
            for (let ix=0; ix<GRID_DIM; ix++) {
                const tile = BuildingSystem.getTile(ix, iz);
                if (tile === 1) {
                    const mesh = new THREE.Mesh(wallGeo, matWall);
                    mesh.position.set((ix+0.5)*GRID_RES - HALF_MAP, 1.5, (iz+0.5)*GRID_RES - HALF_MAP);
                    mesh.castShadow = mesh.receiveShadow = true;
                    this.structureMeshes.add(mesh);
                }
            }
        }
    }

    alivePlayers() {
        let c = this.player.alive ? 1 : 0;
        this.enemies.forEach(e => { if (e.alive) c++; });
        return c;
    }

    getClosestEnemy(me) {
        let best = null, bestD = Infinity;
        const arr = me === this.player ? this.enemies : [this.player, ...this.enemies.filter(e => e !== me)];
        arr.forEach(e => {
            if (!e.alive) return;
            const d = dist3(me.position, e.position);
            if (d < bestD) { bestD = d; best = e; }
        });
        return best;
    }

    raycastShoot(shooter, weapon) {
        const origin = shooter.position.clone();
        const dir = new THREE.Vector3(Math.sin(shooter.yaw), Math.sin(shooter.pitch), Math.cos(shooter.yaw)).normalize();
        if (weapon.spread > 0 && weapon.type !== 'melee') {
            const spread = weapon.spread;
            dir.x += (Math.random()-0.5)*spread;
            dir.y += (Math.random()-0.5)*spread;
            dir.z += (Math.random()-0.5)*spread;
            dir.normalize();
        }
        this.raycaster.set(origin, dir);
        // check against enemies
        for (let e of this.enemies) {
            if (!e.alive || e === shooter) continue;
            const box = new THREE.Box3().setFromObject(e.mesh);
            const intersect = this.raycaster.ray.intersectBox(box, new THREE.Vector3());
            if (intersect) {
                const dmg = weapon.dmg;
                e.damage(dmg);
                if (shooter === this.player) UI.notify('HIT ' + Math.round(dmg));
                if (!e.alive) {
                    shooter.kills++;
                    UI.addKill(`${shooter.name} eliminated ${e.name}`);
                }
                return;
            }
        }
    }

    stormText() {
        if (this.stormTimer > 0) return `Safe Zone (${Math.ceil(this.stormTimer)})`;
        return 'Storm Shrinking';
    }

    buildMaterialName() {
        const costs = STRUCTURE_TYPES[this.player.buildPiece].cost;
        if (costs[0]>0) return 'WOOD';
        if (costs[1]>0) return 'STONE';
        return 'METAL';
    }

    gameLoop() {
        requestAnimationFrame(() => this.gameLoop());
        const dt = Math.min(this.clock.getDelta(), 0.1);
        // input
        this.handleInput(dt);
        // update player
        this.updatePlayer(dt);
        // update enemies
        this.enemies.forEach(e => { if (e.alive) e.update(dt, this); });
        // update storm
        this.updateStorm(dt);
        // sync meshes
        this.player.mesh.position.copy(this.player.position);
        this.player.mesh.visible = this.player.alive;
        this.enemies.forEach(e => {
            e.mesh.position.copy(e.position);
            e.mesh.rotation.y = e.yaw;
            e.mesh.visible = e.alive;
        });
        // refresh structures occasionally
        if (Math.random() < 0.05) this.refreshStructures();
        // storm mesh
        this.stormMesh.scale.set(this.stormRadius/200, 1, this.stormRadius/200);
        // camera follow player
        const p = this.player.position;
        const yaw = this.player.yaw;
        const pitch = this.player.pitch;
        const camDist = 4.5;
        this.camera.position.lerp(new THREE.Vector3(
            p.x - Math.sin(yaw)*camDist*Math.cos(pitch),
            p.y + 2.5 + Math.sin(pitch)*camDist,
            p.z - Math.cos(yaw)*camDist*Math.cos(pitch)
        ), 0.15);
        this.camera.lookAt(p.x, p.y+1.2, p.z);
        this.renderer.render(this.scene, this.camera);
        UI.update(this.player, this);
        // win/lose check
        if (!this.player.alive) {
            UI.notify('ELIMINATED');
        } else if (this.alivePlayers() === 1) {
            UI.notify('VICTORY ROYALE');
        }
    }

    // ... (input handling, player update, storm update will be in the full version)
}
