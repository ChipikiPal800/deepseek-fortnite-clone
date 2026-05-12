// ui.js
const UI = {
    init() {
        this.elements = {
            playerCount: document.getElementById('player-count'),
            stormTimer: document.getElementById('storm-timer'),
            killCount: document.getElementById('kill-count-val'),
            healthVal: document.getElementById('health-value'),
            healthFill: document.getElementById('health-fill'),
            shieldVal: document.getElementById('shield-value'),
            shieldFill: document.getElementById('shield-fill'),
            ammoCurrent: document.getElementById('ammo-current'),
            ammoReserve: document.getElementById('ammo-reserve'),
            matWood: document.getElementById('mat-wood'),
            matStone: document.getElementById('mat-stone'),
            matMetal: document.getElementById('mat-metal'),
            killfeed: document.getElementById('killfeed'),
            buildIndicator: document.getElementById('build-indicator'),
            buildType: document.getElementById('build-type-name'),
            buildMat: document.getElementById('build-mat-name'),
            notification: document.getElementById('notification'),
            damageFlash: document.getElementById('damage-flash'),
            inventorySlots: [0,1,2,3,4].map(i => document.getElementById(`slot-${i}`)),
        };
    },
    update(player, game) {
        // player count
        this.elements.playerCount.textContent = game.alivePlayers();
        this.elements.killCount.textContent = player.kills;
        // health / shield
        this.elements.healthVal.textContent = Math.ceil(player.health);
        this.elements.healthFill.style.width = player.health + '%';
        this.elements.shieldVal.textContent = Math.ceil(player.shield);
        this.elements.shieldFill.style.width = player.shield + '%';
        // ammo
        const w = player.getActiveWeapon();
        const ammo = player.getAmmo();
        this.elements.ammoCurrent.textContent = w.type === 'melee' ? '--' : ammo;
        this.elements.ammoReserve.textContent = w.type === 'melee' ? '--' : (w.capacity || '--');
        // materials
        this.elements.matWood.textContent = player.materials[0];
        this.elements.matStone.textContent = player.materials[1];
        this.elements.matMetal.textContent = player.materials[2];
        // inventory slots
        for (let i = 0; i < 5; i++) {
            const slot = this.elements.inventorySlots[i];
            const wIdx = player.inventory[i];
            if (wIdx !== undefined && WEAPONS[wIdx]) {
                slot.querySelector('.slot-icon').className = 'slot-icon ' + WEAPONS[wIdx].icon;
                slot.querySelector('.slot-name').textContent = WEAPONS[wIdx].desc;
                slot.style.display = '';
            } else {
                slot.style.display = 'none';
            }
            slot.classList.toggle('active', i === player.activeSlot);
        }
        // build mode
        this.elements.buildIndicator.style.display = player.buildMode ? 'block' : 'none';
        if (player.buildMode) {
            this.elements.buildType.textContent = player.buildPiece.toUpperCase();
            this.elements.buildMat.textContent = game.buildMaterialName();
        }
        // storm timer
        this.elements.stormTimer.textContent = game.stormText();
    },
    addKill(msg) {
        const div = document.createElement('div');
        div.className = 'kf-item kf-kill';
        div.textContent = msg;
        this.elements.killfeed.prepend(div);
        if (this.elements.killfeed.children.length > 5) this.elements.killfeed.lastChild.remove();
    },
    flashDamage() {
        this.elements.damageFlash.style.opacity = '1';
        setTimeout(() => this.elements.damageFlash.style.opacity = '0', 120);
    },
    notify(text) {
        this.elements.notification.style.opacity = '1';
        this.elements.notification.textContent = text;
        setTimeout(() => this.elements.notification.style.opacity = '0', 2500);
    }
};
