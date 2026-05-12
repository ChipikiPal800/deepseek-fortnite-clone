// enemy.js
class Enemy extends Player {
    constructor(x, z) {
        super(x, 0, z, false);
        this.name = 'Bot' + Math.floor(Math.random()*100);
        this.inventory = [1, this.randGun()]; // AR + random
        this.activeSlot = 1;
        this.weaponAmmo[1] = 30;
        this.materials = [100, 80, 50];
        this.target = null;
        this.state = 'wander';
        this.stateTimer = 0;
        this.lastSeen = null;
    }

    randGun() { const idx = [2,3,4][Math.floor(Math.random()*3)]; return idx; }

    update(dt, game) {
        if (!this.alive) return;
        super.updateReload(dt);
        // simple AI
        this.target = game.getClosestEnemy(this);
        if (!this.target) return;
        const d = dist3(this.position, this.target.position);
        const angle = lookAt(this.position, this.target.position);
        this.yaw = angle;

        if (d < 60 && this.getActiveWeapon().type !== 'melee') {
            if (d > 15) {
                // move towards
                const speed = 10;
                this.velocity.x = Math.sin(angle) * speed;
                this.velocity.z = Math.cos(angle) * speed;
            } else {
                // strafe
                this.stateTimer += dt;
                const strafe = Math.sin(this.stateTimer * 3);
                this.velocity.x = strafe * 7;
                this.velocity.z = 0;
            }
            if (performance.now()/1000 - this.lastShot > this.getActiveWeapon().fireRate && this.getAmmo() > 0) {
                this.shoot(game);
            }
        } else {
            this.velocity.x = 0;
            this.velocity.z = 0;
        }
        // gravity & ground
        if (!this.onGround) this.velocity.y -= GRAVITY * dt;
        this.position.x += this.velocity.x * dt;
        this.position.z += this.velocity.z * dt;
        this.position.y += this.velocity.y * dt;
        if (this.position.y <= 0) { this.position.y = 0; this.velocity.y = 0; this.onGround = true; }
        // bounds
        this.position.x = clamp(this.position.x, -HALF_MAP+1, HALF_MAP-1);
        this.position.z = clamp(this.position.z, -HALF_MAP+1, HALF_MAP-1);
        // building defense
        if (d < 25 && Math.random() < dt*0.3 && this.materials[0] >= 10) {
            const g = worldToGrid(this.position.x, this.position.z);
            if (BuildingSystem.getTile(g.ix, g.iz) === 0) {
                BuildingSystem.placeStructure(g.ix, g.iz, 'wall', this.materials);
            }
        }
    }
}
