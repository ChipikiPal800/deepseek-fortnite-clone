// utils.js
const MAP_SIZE = 300;
const HALF_MAP = MAP_SIZE / 2;
const GRAVITY = 30;

function clamp(v, min, max) { return Math.max(min, Math.min(max, v)); }
function dist2(a, b) { const dx = a.x - b.x, dz = a.z - b.z; return dx*dx + dz*dz; }
function dist3(a, b) { return Math.sqrt(dist2(a,b) + (a.y-b.y)*(a.y-b.y)); }
function lookAt(a, b) { return Math.atan2(b.x - a.x, b.z - a.z); }

const worldToGrid = (x, z, res=2) => ({
    ix: Math.floor((x + HALF_MAP) / res),
    iz: Math.floor((z + HALF_MAP) / res)
});
