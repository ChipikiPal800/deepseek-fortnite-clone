// lobby.js
(function() {
    const canvas = document.getElementById('lobby-canvas');
    const nameInput = document.getElementById('name-input');
    const playBtn = document.getElementById('play-btn');
    const lobbyDiv = document.getElementById('lobby');
    const gameCanvas = document.getElementById('game-canvas');
    const hud = document.getElementById('hud');

    // Lobby 3D scene
    const scene = new THREE.Scene();
    scene.background = new THREE.Color(0x102040);
    const camera = new THREE.PerspectiveCamera(45, canvas.width/canvas.height, 1, 20);
    camera.position.set(2, 1.5, 4);
    camera.lookAt(0,0.8,0);
    const renderer = new THREE.WebGLRenderer({ canvas, antialias:true });
    renderer.setSize(canvas.width, canvas.height);
    renderer.setPixelRatio(window.devicePixelRatio);

    const light = new THREE.DirectionalLight(0xffffff, 1);
    light.position.set(5,10,5);
    scene.add(light);
    scene.add(new THREE.AmbientLight(0x404060));

    // Simple character (blocky)
    const bodyGeo = new THREE.BoxGeometry(0.6, 1.2, 0.4);
    const bodyMat = new THREE.MeshLambertian({ color: 0xffaa00 });
    const body = new THREE.Mesh(bodyGeo, bodyMat);
    body.position.y = 0.8;
    const headGeo = new THREE.SphereGeometry(0.25, 16,16);
    const head = new THREE.Mesh(headGeo, new THREE.MeshLambertian({ color: 0xffcc66 }));
    head.position.y = 1.5;
    const group = new THREE.Group();
    group.add(body);
    group.add(head);
    scene.add(group);

    function animate() {
        requestAnimationFrame(animate);
        group.rotation.y += 0.01;
        renderer.render(scene, camera);
    }
    animate();

    playBtn.addEventListener('click', () => {
        const name = nameInput.value.trim() || 'PLAYER';
        lobbyDiv.style.display = 'none';
        gameCanvas.style.display = 'block';
        hud.style.display = 'block';
        // start game (defined in game.js)
        startGame(name);
    });
})();
