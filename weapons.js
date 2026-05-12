// weapons.js
const WEAPONS = [
    { name: 'Pickaxe', type: 'melee', dmg: 25, fireRate: 0.5, ammo: Infinity, icon: 'icon-pickaxe', desc: 'Harvesting' },
    { name: 'Assault Rifle', type: 'hitscan', dmg: 33, fireRate: 0.12, ammo: 30, capacity: 30, reload: 2.2, spread: 0.02, icon: 'icon-ar', desc: 'AR' },
    { name: 'Shotgun', type: 'hitscan', dmg: 90, fireRate: 0.8, ammo: 5, capacity: 5, reload: 2.8, spread: 0.08, icon: 'icon-shotgun', desc: 'Shotgun' },
    { name: 'SMG', type: 'hitscan', dmg: 18, fireRate: 0.08, ammo: 30, capacity: 30, reload: 1.8, spread: 0.03, icon: 'icon-smg', desc: 'SMG' },
    { name: 'Sniper Rifle', type: 'hitscan', dmg: 105, fireRate: 1.5, ammo: 5, capacity: 5, reload: 3.0, spread: 0.005, icon: 'icon-sniper', desc: 'Sniper' }
];
