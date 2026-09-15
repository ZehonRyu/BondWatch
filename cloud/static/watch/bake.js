import * as THREE from "three";
import { GLTFLoader } from "./vendor/addons/loaders/GLTFLoader.js";

const W = 160;
const H = 176;
const FRAMES = 12;

function frameFront(camera, center, radius, fill) {
  const vFov = THREE.MathUtils.degToRad(camera.fov);
  const hFov = 2 * Math.atan(Math.tan(vFov / 2) * camera.aspect);
  const dist = Math.max(
    radius / Math.max(Math.tan(vFov / 2), 1e-4) / fill,
    radius / Math.max(Math.tan(hFov / 2), 1e-4) / fill,
    radius * 1.35,
  );
  camera.position.set(center.x, center.y, center.z + dist);
  camera.lookAt(center.x, center.y, center.z);
  camera.near = Math.max(0.05, dist - radius * 1.2);
  camera.far = dist + radius * 8;
  camera.updateProjectionMatrix();
}

async function loadModel() {
  const loader = new GLTFLoader();
  const gltf = await loader.loadAsync(new URL("./models/lumi.glb?v=26", import.meta.url).href);
  const model = gltf.scene;
  const wrap = new THREE.Group();
  wrap.add(model);
  wrap.updateMatrixWorld(true);
  wrap.traverse((obj) => {
    if (obj.isSkinnedMesh && obj.skeleton) obj.skeleton.update();
    if (obj.isMesh) {
      obj.frustumCulled = false;
      if (obj.material) obj.material.side = THREE.DoubleSide;
    }
  });
  const raw = new THREE.Box3().setFromObject(wrap);
  const size = raw.getSize(new THREE.Vector3());
  const center = raw.getCenter(new THREE.Vector3());
  model.position.sub(center);
  wrap.scale.setScalar(1.42 / Math.max(size.x, size.y, size.z, 0.001));
  wrap.updateMatrixWorld(true);
  const fitted = new THREE.Box3().setFromObject(wrap);
  wrap.position.y -= fitted.min.y;
  wrap.updateMatrixWorld(true);
  const mixer = gltf.animations && gltf.animations.length ? new THREE.AnimationMixer(model) : null;
  const clip = mixer ? mixer.clipAction(gltf.animations[0]) : null;
  if (clip) {
    clip.play();
    mixer.update(0);
  }
  return { wrap, mixer, clip, duration: gltf.animations[0] ? gltf.animations[0].duration : 1 };
}

async function main() {
  const canvas = document.getElementById("bake");
  const renderer = new THREE.WebGLRenderer({
    canvas,
    antialias: true,
    alpha: false,
    preserveDrawingBuffer: true,
  });
  renderer.setPixelRatio(1);
  renderer.setSize(W, H, false);
  renderer.setClearColor(0x102a44, 1);
  renderer.outputColorSpace = THREE.SRGBColorSpace;

  const scene = new THREE.Scene();
  const camera = new THREE.PerspectiveCamera(32, W / H, 0.05, 20);
  scene.add(new THREE.HemisphereLight(0xffffff, 0x223355, 1.15));
  const key = new THREE.DirectionalLight(0xfff2d8, 1.55);
  key.position.set(0.4, 1.6, 2.4);
  scene.add(key);
  const fill = new THREE.DirectionalLight(0xb8d4ff, 0.55);
  fill.position.set(-1.2, 0.4, 1.6);
  scene.add(fill);

  const { wrap, mixer, clip, duration } = await loadModel();
  scene.add(wrap);

  function renderAt(t) {
    if (mixer && clip) {
      clip.paused = true;
      mixer.setTime(((t % duration) + duration) % duration);
    }
    wrap.updateMatrixWorld(true);
    wrap.traverse((obj) => {
      if (obj.isSkinnedMesh && obj.skeleton) obj.skeleton.update();
    });
    const box = new THREE.Box3().setFromObject(wrap);
    const center = box.getCenter(new THREE.Vector3());
    const size = box.getSize(new THREE.Vector3());
    const radius = Math.max(size.x, size.y, size.z) * 0.5;
    frameFront(camera, center, radius, 0.82);
    renderer.render(scene, camera);
  }

  window.BakeLumi = {
    frames: FRAMES,
    width: W,
    height: H,
    duration,
    capture(index) {
      const t = (index / FRAMES) * duration;
      renderAt(t);
      return {
        index,
        t,
        dataUrl: canvas.toDataURL("image/png"),
      };
    },
    captureAll() {
      const out = [];
      for (let i = 0; i < FRAMES; i += 1) out.push(this.capture(i));
      return out;
    },
  };
  renderAt(0);
  window.dispatchEvent(new Event("bake-ready"));
}

main().catch((err) => {
  console.error(err);
  window.BakeError = String(err);
});
