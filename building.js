// building.js
const GRID_RES = 2;
const GRID_DIM = Math.ceil(MAP_SIZE / GRID_RES);
const STRUCTURE_TYPES = {
    wall: { id: 1, cost: [10,0,0], name: 'WALL' },
    floor: { id: 2, cost: [0,10,0], name: 'FLOOR' },
    ramp: { id: 3, cost: [0,0,10], name: 'RAMP' },
    roof: { id: 4, cost: [5,5,0], name: 'ROOF' }
};

const BuildingSystem = {
    grid: new Uint8Array(GRID_DIM * GRID_DIM), // 0=empty, 1=wall, 2=floor, 3=ramp, 4=roof
    init() {
        this.grid.fill(0);
    },
    getIndex(ix, iz) {
        if (ix<0||ix>=GRID_DIM||iz<0||iz>=GRID_DIM) return -1;
        return iz * GRID_DIM + ix;
    },
    getTile(ix, iz) {
        const i = this.getIndex(ix, iz);
        return i < 0 ? 0 : this.grid[i];
    },
    setTile(ix, iz, type) {
        const i = this.getIndex(ix, iz);
        if (i >= 0) this.grid[i] = type;
    },
    placeStructure(ix, iz, type, materials) {
        if (this.getTile(ix, iz) !== 0) return false;
        const cost = STRUCTURE_TYPES[type].cost;
        if (materials[0] < cost[0] || materials[1] < cost[1] || materials[2] < cost[2]) return false;
        materials[0] -= cost[0]; materials[1] -= cost[1]; materials[2] -= cost[2];
        this.setTile(ix, iz, STRUCTURE_TYPES[type].id);
        return true;
    }
};
