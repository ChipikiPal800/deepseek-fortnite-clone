// player.js
class Player {
    constructor(x, y, z, isLocal = false) {
        this.position = new THREE.Vector3(x, y, z);
        this.velocity = new THREE.Vector3();
        this.yaw = 0;
        this.pitch = 0;
        this.health = 100;
        this.shield = 100;
        this.alive = true;
        this.onGround = false;
        this.isLocal = isLocal;
        this.inventory = [0]; // indices into WEAPONS array, start with pickaxe
        this.activeSlot = 0;
        this.materials = [200, 150, 75]; // wood, stone, metal
        this.buildMode = false;
        this.buildPiece = 'wall';
        this.kills = 0;
        this.name = isLocal ? 'YOU' : 'Bot';
        this.lastShot = 0;
        this.reloading = false;
        this.reloadTimer = 0;
        this.weaponAmmo = [Infinity,30,5,30,5]; // parallel to inventory weapons
    }

    getActiveWeapon() {
        return WEAPONS[this.inventory[this.activeSlot]];
    }

    getAmmo() {
        const w = this.getActiveWeapon();
        if (w.type === 'melee') return Infinity;
        return this.weaponAmmo[this.activeSlot];
    }

    shoot(game) {
        const now = performance.now() / 1000;
        const w = this.getActiveWeapon();
        if (w.type === 'melee') {
            // harvesting handled in game
            this.lastShot = now;
            return;
        }
        if (this.reloading) return;
        const ammo = this.getAmmo();
        if (ammo <= 0) { this.startReload(); return; }
        if (now - this.lastShot < w.fireRate) return;
        this.lastShot = now;
        this.weaponAmmo[this.activeSlot]--;
        // perform hitscan
        game.raycastShoot(this, w);
    }

    startReload() {
        if (this.reloading) return;
        const w = this.getActiveWeapon();
        if (w.type === 'melee' || w.capacity === undefined) return;
        if (this.weaponAmmo[this.activeSlot] >= w.capacity) return;
        this.reloading = true;
        this.reloadTimer = w.reload;
    }

    updateReload(dt) {
        if (this.reloading) {
            this.reloadTimer -= dt;
            if (this.reloadTimer <= 0) {
                const w = this.getActiveWeapon();
                this.weaponAmmo[this.activeSlot] = w.capacity;
                this.reloading = false;
            }
        }
    }

    addWeapon(weaponIndex) {
        if (this.inventory.length >= 5) return false;
        this.inventory.push(weaponIndex);
        this.weaponAmmo[this.inventory.length-1] = WEAPONS[weaponIndex].capacity || Infinity;
        return true;
    }

    damage(amount) {
        if (this.shield > 0) {
            if (this.shield >= amount) { this.shield -= amount; return; }
            amount -= this.shield;
            this.shield = 0;
        }
        this.health -= amount;
        if (this.health <= 0) {
            this.health = 0;
            this.alive = false;
        }
    }
}
