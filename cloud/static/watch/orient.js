(function () {
  let tilt = null;

  function fromTilt(beta, gamma) {
    if (beta == null || gamma == null) return null;
    return Math.abs(gamma) >= 40;
  }

  function isLandscape() {
    if (tilt && Date.now() - tilt.at < 2500) return tilt.landscape;
    if (typeof window.orientation === "number") {
      return Math.abs(window.orientation) === 90;
    }
    const mq = window.matchMedia("(orientation: landscape)").matches;
    const aspect = window.innerWidth > window.innerHeight + 24;
    if (mq || aspect) return true;
    if (!mq && window.innerWidth < window.innerHeight) return false;
    const type = (screen.orientation && screen.orientation.type) || "";
    return type.indexOf("landscape") === 0;
  }

  function apply() {
    const wide = isLandscape();
    document.documentElement.classList.toggle("is-landscape", wide);
    if (document.body) document.body.classList.toggle("is-landscape", wide);
    const badge = document.getElementById("orient-badge");
    if (badge) badge.textContent = wide ? "横屏" : "竖屏";
    window.dispatchEvent(new CustomEvent("watch-orient", { detail: { landscape: wide } }));
    return wide;
  }

  function onDeviceOrient(event) {
    const land = fromTilt(event.beta, event.gamma);
    if (land == null) return;
    const prev = tilt && tilt.landscape;
    tilt = { landscape: land, at: Date.now(), beta: event.beta, gamma: event.gamma };
    if (prev !== land) apply();
  }

  async function enableMotion() {
    try {
      if (
        typeof DeviceOrientationEvent !== "undefined" &&
        typeof DeviceOrientationEvent.requestPermission === "function"
      ) {
        const granted = await DeviceOrientationEvent.requestPermission();
        if (granted !== "granted") return false;
      }
    } catch (_) {
      return false;
    }
    window.addEventListener("deviceorientation", onDeviceOrient, true);
    window.addEventListener("deviceorientationabsolute", onDeviceOrient, true);
    return true;
  }

  window.isWatchLandscape = isLandscape;
  window.syncWatchOrient = apply;
  window.enableWatchMotion = enableMotion;
  window.watchOrientDebug = function () {
    return {
      landscape: isLandscape(),
      tilt,
      orientation: typeof window.orientation === "number" ? window.orientation : null,
      screen: screen.orientation && screen.orientation.type,
      mq: window.matchMedia("(orientation: landscape)").matches,
      size: [window.innerWidth, window.innerHeight],
    };
  };

  window.addEventListener("orientationchange", function () {
    setTimeout(apply, 50);
    setTimeout(apply, 350);
  });
  window.addEventListener("resize", apply);
  if (window.visualViewport) window.visualViewport.addEventListener("resize", apply);
  const mq = window.matchMedia("(orientation: landscape)");
  if (mq.addEventListener) mq.addEventListener("change", apply);
  else if (mq.addListener) mq.addListener(apply);
  if (screen.orientation && screen.orientation.addEventListener) {
    screen.orientation.addEventListener("change", apply);
  }

  window.addEventListener("deviceorientation", onDeviceOrient, true);
  document.addEventListener(
    "click",
    function once() {
      document.removeEventListener("click", once, true);
      enableMotion();
    },
    true,
  );
  document.addEventListener(
    "touchend",
    function once() {
      document.removeEventListener("touchend", once, true);
      enableMotion();
    },
    true,
  );

  apply();
})();
