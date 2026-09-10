import * as THREE from "three";
import { GLTFLoader } from "/vendor/addons/loaders/GLTFLoader.js";

const faces = {
  idle: 0x5d9cec,
  listen: 0x2ecc71,
  think: 0xc39bd3,
  speak: 0xf5b041,
  quiet: 0xb0b0b0,
  silent: 0x6e6e6e,
  alarm: 0xe74c3c,
  offline: 0x7f8c8d,
};

function skinMat(color, tint = true) {
  const mat = new THREE.MeshStandardMaterial({
    color,
    roughness: 0.42,
    metalness: 0.04,
  });
  mat.userData.tint = tint;
  return mat;
}

function makeProcedural() {
  const root = new THREE.Group();
  const skin = skinMat(0x5d9cec);
  const ink = skinMat(0x1a1408, false);

  const head = new THREE.Group();
  head.name = "head";
  const skull = new THREE.Mesh(new THREE.SphereGeometry(0.42, 28, 20), skin);
  skull.castShadow = true;
  head.add(skull);

  const earGeo = new THREE.ConeGeometry(0.13, 0.46, 8);
  const earL = new THREE.Mesh(earGeo, skin);
  earL.position.set(-0.3, 0.32, -0.02);
  earL.rotation.z = 0.62;
  const earR = new THREE.Mesh(earGeo, skin);
  earR.position.set(0.3, 0.32, -0.02);
  earR.rotation.z = -0.62;
  head.add(earL, earR);

  const spark = new THREE.Mesh(new THREE.OctahedronGeometry(0.06, 0), skinMat(0xfff6c2, false));
  spark.position.set(0, 0.48, 0.08);
  head.add(spark);

  const eyeL = new THREE.Mesh(new THREE.SphereGeometry(0.05, 12, 10), ink);
  eyeL.position.set(-0.13, 0.05, 0.36);
  const eyeR = new THREE.Mesh(new THREE.SphereGeometry(0.05, 12, 10), ink);
  eyeR.position.set(0.13, 0.05, 0.36);
  const blush = skinMat(0xff7896, false);
  blush.transparent = true;
  blush.opacity = 0.35;
  const cheekL = new THREE.Mesh(new THREE.SphereGeometry(0.06, 10, 8), blush);
  cheekL.position.set(-0.22, -0.04, 0.32);
  cheekL.scale.set(1.2, 0.7, 0.5);
  const cheekR = cheekL.clone();
  cheekR.position.x = 0.22;
  const mouth = new THREE.Mesh(new THREE.SphereGeometry(0.07, 12, 10), ink);
  mouth.position.set(0, -0.14, 0.37);
  mouth.scale.set(1, 0.22, 0.55);
  head.add(eyeL, eyeR, cheekL, cheekR, mouth);
  head.position.y = 0.72;
  root.add(head);

  const body = new THREE.Mesh(new THREE.SphereGeometry(0.2, 16, 12), skin);
  body.scale.set(0.95, 1.1, 0.8);
  body.position.y = 0.22;
  root.add(body);

  function makeArm(sign) {
    const arm = new THREE.Group();
    const upper = new THREE.Mesh(new THREE.CapsuleGeometry(0.05, 0.2, 4, 8), skin);
    upper.position.y = -0.14;
    const forearm = new THREE.Group();
    forearm.position.y = -0.28;
    const lower = new THREE.Mesh(new THREE.CapsuleGeometry(0.045, 0.18, 4, 8), skin);
    lower.position.y = -0.12;
    const hand = new THREE.Mesh(new THREE.SphereGeometry(0.062, 12, 10), skin);
    hand.position.set(0, -0.26, 0.02);
    forearm.add(lower, hand);
    arm.add(upper, forearm);
    arm.position.set(sign * 0.24, 0.34, 0.02);
    arm.userData = { forearm, hand, sign };
    return arm;
  }

  const armL = makeArm(-1);
  const armR = makeArm(1);
  root.add(armL, armR);

  function makeLeg(sign) {
    const leg = new THREE.Mesh(new THREE.CapsuleGeometry(0.045, 0.14, 4, 8), skin);
    leg.position.set(sign * 0.08, -0.02, 0);
    return leg;
  }
  root.add(makeLeg(-1), makeLeg(1));

  root.userData = { head, eyeL, eyeR, mouth, spark, armL, armR, skin, procedural: true };
  return root;
}

function applySkin(root, hex) {
  const color = new THREE.Color(hex);
  root.traverse((obj) => {
    if (!obj.isMesh || !obj.material || !obj.material.color) return;
    if (root.userData.procedural && !obj.material.userData.tint) return;
    if (!root.userData.procedural && obj.material.userData && obj.material.userData.skipTint) return;
    obj.material.color.copy(color);
  });
}

function pick(root, name) {
  let found = null;
  root.traverse((obj) => {
    if (obj.name === name) found = obj;
  });
  return found;
}

function saveRest(obj) {
  if (!obj) return;
  obj.userData.restQ = obj.quaternion.clone();
  obj.userData.restP = obj.position.clone();
}

function addLocal(obj, x, y, z) {
  if (!obj || !obj.userData.restQ) return;
  obj.quaternion.copy(obj.userData.restQ);
  obj.quaternion.multiply(new THREE.Quaternion().setFromEuler(new THREE.Euler(x, y, z)));
}

function addWorld(obj, ax, ay, az) {
  if (!obj || !obj.userData.restQ) return;
  obj.quaternion.copy(obj.userData.restQ);
  const parent = obj.parent;
  if (!parent) {
    addLocal(obj, ax, ay, az);
    return;
  }
  parent.updateWorldMatrix(true, false);
  const parentQ = parent.getWorldQuaternion(new THREE.Quaternion());
  const worldRest = parentQ.clone().multiply(obj.userData.restQ);
  const extra = new THREE.Quaternion().setFromEuler(new THREE.Euler(ax, ay, az, "YXZ"));
  const worldQ = extra.multiply(worldRest);
  obj.quaternion.copy(parentQ.clone().invert().multiply(worldQ));
}

function expandNamed(box, root, names) {
  names.forEach((name) => {
    const obj = pick(root, name);
    if (obj) box.expandByObject(obj);
  });
  return box;
}

function frameFront(camera, center, radius, fill) {
  const vFov = THREE.MathUtils.degToRad(camera.fov);
  const hFov = 2 * Math.atan(Math.tan(vFov / 2) * camera.aspect);
  const dist = Math.max(
    radius / Math.max(Math.tan(vFov / 2), 1e-4) / fill,
    radius / Math.max(Math.tan(hFov / 2), 1e-4) / fill,
    radius * 1.4,
  );
  camera.position.set(center.x, center.y, center.z + dist);
  camera.lookAt(center.x, center.y, center.z);
  camera.near = Math.max(0.03, dist - radius * 0.9);
  camera.far = dist + radius * 10;
  camera.updateProjectionMatrix();
  return dist > radius * 1.25;
}

function bindBoy(scene) {
  const bones = {
    hips: pick(scene, "hips"),
    head: pick(scene, "Head"),
    neck: pick(scene, "Neck"),
    armL: pick(scene, "leftUpperArm"),
    armR: pick(scene, "rightUpperArm"),
    foreL: pick(scene, "leftLowerArm"),
    foreR: pick(scene, "rightLowerArm"),
    handL: pick(scene, "leftHand"),
    handR: pick(scene, "rightHand"),
    eyeL: pick(scene, "bob_eyel") || pick(scene, "EyeJoint_L"),
    eyeR: pick(scene, "bob_eyer") || pick(scene, "EyeJoint_R"),
    tongue: pick(scene, "bob_Shetou"),
  };
  Object.values(bones).forEach(saveRest);
  scene.userData.bones = bones;
  scene.userData.procedural = false;
  return scene;
}

async function loadBoy() {
  const gltf = await new GLTFLoader().loadAsync("/models/boybase.glb");
  const model = gltf.scene;
  model.updateMatrixWorld(true);
  const raw = new THREE.Box3().setFromObject(model);
  const size = raw.getSize(new THREE.Vector3());
  const center = raw.getCenter(new THREE.Vector3());
  const wrap = new THREE.Group();
  wrap.add(model);
  model.position.sub(center);
  wrap.scale.setScalar(1.42 / Math.max(size.x, size.y, size.z, 0.001));
  wrap.updateMatrixWorld(true);
  const fitted = new THREE.Box3().setFromObject(wrap);
  wrap.position.y -= fitted.min.y;
  wrap.updateMatrixWorld(true);
  bindBoy(model);
  wrap.userData.procedural = false;
  wrap.userData.bones = model.userData.bones;
  wrap.userData.height = new THREE.Box3().setFromObject(wrap).getSize(new THREE.Vector3()).y;
  model.traverse((obj) => {
    if (obj.isMesh) {
      obj.frustumCulled = false;
      if (obj.material) obj.material.side = THREE.FrontSide;
    }
  });
  return wrap;
}

export async function mountElf3D(canvas) {
  const renderer = new THREE.WebGLRenderer({
    canvas,
    antialias: true,
    alpha: true,
  });
  renderer.setPixelRatio(Math.min(window.devicePixelRatio || 1, 2));
  renderer.setClearColor(0x000000, 0);
  renderer.outputColorSpace = THREE.SRGBColorSpace;

  const scene = new THREE.Scene();
  const camera = new THREE.PerspectiveCamera(32, 1, 0.1, 20);
  camera.position.set(0, 0.35, 3.1);

  scene.add(new THREE.HemisphereLight(0xffffff, 0x223355, 1.15));
  const key = new THREE.DirectionalLight(0xfff2d8, 1.55);
  key.position.set(0.4, 1.6, 2.4);
  scene.add(key);
  const fill = new THREE.DirectionalLight(0xb8d4ff, 0.55);
  fill.position.set(-1.2, 0.4, 1.6);
  scene.add(fill);

  const clipPlane = new THREE.Plane(new THREE.Vector3(0, 0, -1), 0);
  renderer.localClippingEnabled = true;

  let elf = makeProcedural();
  scene.add(elf);
  try {
    const boy = await loadBoy();
    scene.remove(elf);
    elf = boy;
    scene.add(elf);
  } catch (err) {
    console.warn("boybase.glb", err);
  }

  elf.traverse((obj) => {
    if (obj.isMesh && obj.material) {
      obj.material.clippingPlanes = [clipPlane];
      obj.material.clipShadows = true;
    }
  });

  let emotion = "idle";
  let screenOn = true;
  const baseY = elf.position.y;
  const baseRot = elf.rotation.clone();

  function resize() {
    const w = canvas.clientWidth || 240;
    const h = canvas.clientHeight || 280;
    renderer.setSize(w, h, false);
    camera.aspect = w / Math.max(h, 1);
    camera.updateProjectionMatrix();
  }

  function landscape() {
    const wide = window.isWatchLandscape ? window.isWatchLandscape() : window.matchMedia("(orientation: landscape)").matches;
    document.documentElement.classList.toggle("is-landscape", wide);
    document.body.classList.toggle("is-landscape", wide);
    return wide;
  }

  let lastWide = null;

  function pose(t) {
    const wide = landscape();
    if (wide !== lastWide) {
      lastWide = wide;
      resize();
    }
    const u = elf.userData;
    const b = u.bones || {};

    if (wide) {
      elf.position.set(0, baseY, 0);
      elf.rotation.set(0.12, 0, 0);
      if (u.procedural) {
        if (u.head && u.head.userData.restQ) u.head.quaternion.copy(u.head.userData.restQ);
        u.head.rotation.x = 0.22;
        u.armL.rotation.set(0.15, 0.05, 0.35);
        u.armR.rotation.set(0.15, -0.05, -0.35);
        u.armL.userData.forearm.rotation.set(0.2, 0, 0);
        u.armR.userData.forearm.rotation.set(0.2, 0, 0);
      } else {
        addWorld(b.head, 0.22, 0, 0);
        addWorld(b.neck, 0.08, 0, 0);
        addWorld(b.hips, 0, 0, 0);
        addWorld(b.armL, 0.05, 0.05, 0.08);
        addWorld(b.armR, 0.05, -0.05, -0.08);
        addWorld(b.foreL, 0, 0, 0);
        addWorld(b.foreR, 0, 0, 0);
        addWorld(b.handL, 0, 0, 0);
        addWorld(b.handR, 0, 0, 0);
      }
    } else {
      const hop = Math.abs(Math.sin(t * 3.4)) * 0.045;
      elf.position.set(0, baseY + hop, 0);
      elf.rotation.set(baseRot.x, Math.sin(t * 1.4) * 0.08, 0);
      if (u.procedural) {
        u.armL.rotation.set(0.25, 0.1, 0.55);
        u.armL.userData.forearm.rotation.set(0.15, 0, 0);
        u.armR.rotation.set(0.1, 0, -0.35 + Math.sin(t * 7) * 1.15);
        u.armR.userData.forearm.rotation.set(0.2, 0, 0);
        u.armL.userData.hand.position.z = 0.02;
        u.armR.userData.hand.position.z = 0.02;
      } else {
        addWorld(b.head, 0, 0, 0);
        addWorld(b.neck, 0, 0, 0);
        addWorld(b.hips, 0, Math.sin(t * 1.4) * 0.05, 0);
        addWorld(b.armL, 0.15, 0.1, 0.2);
        addWorld(b.armR, 0.1, -0.15, -0.25 + Math.sin(t * 7) * 1.0);
        addWorld(b.foreL, 0.1, 0, 0.1);
        addWorld(b.foreR, 0.2, 0, 0.25);
        addWorld(b.handL, 0, 0, 0);
        addWorld(b.handR, 0, 0, 0);
      }
    }

    elf.updateMatrixWorld(true);

    const center = new THREE.Vector3();
    const target = new THREE.Box3();
    let radius = 0.7;
    if (wide) {
      const eyeL = pick(elf, "EyeJoint_L") || b.eyeL || u.eyeL;
      const eyeR = pick(elf, "EyeJoint_R") || b.eyeR || u.eyeR;
      const headEnd = pick(elf, "HeadEnd_M") || pick(elf, "FaceJoint_M");
      if (eyeL && eyeR) {
        const p1 = eyeL.getWorldPosition(new THREE.Vector3());
        const p2 = eyeR.getWorldPosition(new THREE.Vector3());
        center.copy(p1).add(p2).multiplyScalar(0.5);
        if (headEnd) center.lerp(headEnd.getWorldPosition(new THREE.Vector3()), 0.2);
        radius = Math.max(p1.distanceTo(p2) * 1.45, 0.16);
        center.y -= radius * 0.18;
      } else {
        expandNamed(target, elf, ["bob_face", "bob_eyel", "bob_eyer"]);
        if (target.isEmpty() && (b.head || u.head)) target.expandByObject(b.head || u.head);
        target.getCenter(center);
        const size = target.getSize(new THREE.Vector3());
        radius = Math.max(size.x, size.y) * 0.42;
      }
    } else {
      target.setFromObject(elf);
      if (target.isEmpty()) {
        target.setFromCenterAndSize(new THREE.Vector3(0, 0.7, 0), new THREE.Vector3(0.6, 1.4, 0.6));
      }
      target.getCenter(center);
      const size = target.getSize(new THREE.Vector3());
      radius = Math.max(size.x, size.y, size.z) * 0.5;
    }

    camera.fov = wide ? 26 : 32;
    const outside = frameFront(camera, center, radius, wide ? 1.22 : 0.78);
    if (!outside) {
      camera.position.z = center.z + radius * 1.5;
      camera.near = radius * 0.4;
      camera.updateProjectionMatrix();
    }

    const glassZ = camera.position.z - Math.max(camera.near * 1.02, 0.04);
    clipPlane.set(new THREE.Vector3(0, 0, -1), glassZ);
    renderer.clippingPlanes = [clipPlane];

    const blink = t % 3.2 > 3.04;
    const sleepy = wide ? 0.34 + Math.abs(Math.sin(t * 1.1)) * 0.1 : 1;
    const eyeY = wide ? sleepy : blink ? 0.1 : 1;
    if (u.eyeL && u.eyeR) {
      u.eyeL.scale.y = eyeY;
      u.eyeR.scale.y = eyeY;
    }
    if (b.eyeL && b.eyeR) {
      b.eyeL.scale.y = wide ? sleepy : blink ? 0.08 : 1;
      b.eyeR.scale.y = wide ? sleepy : blink ? 0.08 : 1;
    }
    const talk = emotion === "speak" || emotion === "listen" || emotion === "alarm";
    if (u.mouth) {
      u.mouth.scale.y = talk ? 0.22 + Math.abs(Math.sin(t * 11)) * 0.85 : wide ? 0.16 : 0.22;
      u.mouth.scale.x = talk ? 0.85 : 1;
    }
    if (b.tongue && b.tongue.userData.restP) {
      b.tongue.scale.y = talk ? 1 + Math.abs(Math.sin(t * 11)) * 0.35 : 1;
    }
    if (u.spark) {
      const s = 0.85 + 0.25 * Math.sin(t * 4);
      u.spark.scale.setScalar(s);
      u.spark.rotation.y = t;
    }
  }

  function loop(now) {
    const t = now / 1000;
    canvas.style.opacity = screenOn ? "1" : "0";
    pose(t);
    renderer.render(scene, camera);
    requestAnimationFrame(loop);
  }

  resize();
  requestAnimationFrame(loop);
  window.addEventListener("resize", resize);
  window.addEventListener("orientationchange", () => {
    setTimeout(resize, 60);
    setTimeout(resize, 320);
  });
  window.addEventListener("watch-orient", resize);
  if (screen.orientation && screen.orientation.addEventListener) {
    screen.orientation.addEventListener("change", () => setTimeout(resize, 60));
  }

  window.Elf3D = {
    setEmotion(id) {
      emotion = id || "idle";
      if (elf.userData.procedural) applySkin(elf, faces[emotion] || faces.idle);
    },
    setSkin(cssColor) {
      if (elf.userData.procedural && cssColor) applySkin(elf, cssColor);
    },
    setOn(on) {
      screenOn = on;
    },
    resize,
    debug() {
      return {
        procedural: elf.userData.procedural,
        height: elf.userData.height,
        landscape: landscape(),
        orient: window.watchOrientDebug ? window.watchOrientDebug() : null,
      };
    },
  };
  window.dispatchEvent(new Event("elf3d-ready"));
}

const canvas = document.getElementById("elf3d");
if (canvas) mountElf3D(canvas).catch((err) => console.warn("elf3d", err));
