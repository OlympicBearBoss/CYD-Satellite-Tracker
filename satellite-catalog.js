/* Live public satellite catalog. Orbit positions are computed locally; no hardware commands are sent. */
(() => {
  const sources = [
    { group: "stations", label: "Станции" },
    { group: "weather", label: "Метеоспутники" },
    { group: "amateur", label: "Любительские" }
  ];
  let records = [];
  let selected = null;
  let satelliteLibReady = null;
  const $ = id => document.getElementById(id);
  const esc = value => String(value ?? "").replace(/[&<>"']/g, c => ({"&":"&amp;","<":"&lt;",">":"&gt;",'"':"&quot;","'":"&#39;"}[c]));
  const status = value => { if ($("orbitFeedStatus")) $("orbitFeedStatus").textContent = value; };
  async function loadSatelliteLib() {
    if (window.satellite) return window.satellite;
    if (!satelliteLibReady) satelliteLibReady = new Promise((resolve, reject) => {
      const s = document.createElement("script");
      s.src = "https://unpkg.com/satellite.js@6.0.1/dist/satellite.min.js";
      s.onload = () => window.satellite ? resolve(window.satellite) : reject(new Error("Библиотека расчёта орбит не загрузилась"));
      s.onerror = () => reject(new Error("Не удалось загрузить satellite.js"));
      document.head.appendChild(s);
    });
    return satelliteLibReady;
  }
  async function refresh() {
    status("Загрузка…");
    $("liveSatelliteList").innerHTML = '<div class="note">Получение орбитальных элементов CelesTrak…</div>';
    try {
      const responses = await Promise.all(sources.map(async source => {
        const url = "https://celestrak.org/NORAD/elements/gp.php?GROUP=" + encodeURIComponent(source.group) + "&FORMAT=json";
        const response = await fetch(url, { cache: "no-store" });
        if (!response.ok) throw new Error(source.group + ": HTTP " + response.status);
        const data = await response.json();
        return data.map(item => ({...item, _category: source.group}));
      }));
      const byId = new Map();
      responses.flat().forEach(item => {
        const id = String(item.NORAD_CAT_ID ?? item. NORAD_CAT_ID);
        if (id && item.TLE_LINE1 && item.TLE_LINE2) byId.set(id, item);
      });
      records = [...byId.values()];
      $("orbitUpdatedAt").textContent = "Получено: " + new Date().toLocaleString();
      status(records.length + " объектов");
      render();
      if (!records.length) throw new Error("Источник не вернул записи с TLE");
    } catch (error) {
      status("Нет связи с источником");
      $("liveSatelliteList").innerHTML = '<div class="note">Не удалось загрузить каталог: ' + esc(error.message) + '. Проверьте интернет-соединение или повторите запрос.</div>';
    }
  }
  function categoryMatch(item) {
    const category = $("satCategory").value;
    return category === "all" || category === "stations" && item._category === "stations" ||
      category === "weather" && item._category === "weather" || category === "amateur" && item._category === "amateur";
  }
  function render() {
    const query = $("satSearch").value.trim().toLowerCase();
    const visible = records.filter(item => categoryMatch(item) &&
      (String(item.OBJECT_NAME).toLowerCase().includes(query) || String(item.NORAD_CAT_ID).includes(query)));
    $("liveSatelliteList").innerHTML = visible.slice(0, 100).map(item =>
      '<button type="button" class="object-row" data-live-id="' + esc(item.NORAD_CAT_ID) + '" style="width:100%;text-align:left;color:inherit;cursor:pointer"><span><b>' +
      esc(item.OBJECT_NAME) + '</b><small>NORAD ' + esc(item.NORAD_CAT_ID) + ' · ' + esc(item._category) + '</small></span><strong>Выбрать ↗</strong></button>'
    ).join("") || '<div class="note">По запросу ничего не найдено.</div>';
    $("liveSatelliteList").querySelectorAll("[data-live-id]").forEach(button => button.addEventListener("click", () => {
      selected = records.find(item => String(item.NORAD_CAT_ID) === button.dataset.liveId);
      showDetails().catch(error => { $("liveSatelliteDetails").textContent = error.message; });
    }));
  }
  async function showDetails() {
    if (!selected) return;
    const epoch = selected.EPOCH ? new Date(selected.EPOCH).toLocaleString() : "не указана";
    $("liveSatelliteDetails").innerHTML = '<b>' + esc(selected.OBJECT_NAME) + '</b> · NORAD ' + esc(selected.NORAD_CAT_ID) +
      '<br>Эпоха орбитальных элементов: ' + esc(epoch) +
      '<br>Наклонение: ' + esc(selected.INCLINATION) + '° · Эксцентриситет: ' + esc(selected.ECCENTRICITY) +
      '<br>Среднее движение: ' + esc(selected.MEAN_MOTION) + ' об/сутки' +
      '<div class="button-row"><button class="btn btn-primary" id="calculateSelectedPosition">Рассчитать положение сейчас</button></div>' +
      '<div id="selectedPositionResult" style="margin-top:9px">Введите координаты станции и нажмите расчёт.</div>';
    $("calculateSelectedPosition").addEventListener("click", calculatePosition);
  }
  async function calculatePosition() {
    const lat = Number($("observerLat").value), lon = Number($("observerLon").value);
    if (!Number.isFinite(lat) || !Number.isFinite(lon) || lat < -90 || lat > 90 || lon < -180 || lon > 180) {
      $("selectedPositionResult").textContent = "Укажите корректные широту (−90…90°) и долготу (−180…180°).";
      return;
    }
    try {
      const sat = await loadSatelliteLib();
      const satrec = sat.twoline2satrec(selected.TLE_LINE1, selected.TLE_LINE2);
      const now = new Date();
      const positionAndVelocity = sat.propagate(satrec, now);
      if (!positionAndVelocity.position) throw new Error("Не удалось распространить орбиту для выбранного времени.");
      const gmst = sat.gstime(now);
      const positionEcf = sat.eciToEcf(positionAndVelocity.position, gmst);
      const observer = { latitude: sat.degreesToRadians(lat), longitude: sat.degreesToRadians(lon), height: 0 };
      const look = sat.ecfToLookAngles(observer, positionEcf);
      const az = (sat.radiansToDegrees(look.azimuth) + 360) % 360;
      const el = sat.radiansToDegrees(look.elevation);
      const range = look.rangeSat;
      $("selectedPositionResult").innerHTML = '<b>Расчёт на ' + now.toISOString() + ' UTC</b><br>Азимут: ' + az.toFixed(1) +
        '° · Угол места: ' + el.toFixed(1) + '°<br>Дальность: ' + range.toFixed(1) + ' км' +
        '<br><span style="color:#8da5bc">Положение расчётное; отрицательный угол места означает, что спутник ниже горизонта.</span>' +
        '<div class="button-row"><button class="btn" id="applyPositionToDemo">Перенести углы в демонстрационное управление</button></div>';
      $("applyPositionToDemo").addEventListener("click", () => {
        const azInput = $("azRange"), elInput = $("elRange");
        if (azInput && elInput && el >= 0) {
          azInput.value = az.toFixed(1); elInput.value = Math.min(90, Math.max(0, el)).toFixed(1);
          azInput.dispatchEvent(new Event("input", {bubbles:true}));
          elInput.dispatchEvent(new Event("input", {bubbles:true}));
          $("controlStatus").textContent = "Расчётные углы " + selected.OBJECT_NAME + " перенесены в демо-панель. Команды на моторы не отправлялись.";
        } else if (el < 0) {
          $("selectedPositionResult").insertAdjacentHTML("beforeend", "<br>Спутник не виден над горизонтом — наведение не перенесено.");
        }
      });
    } catch (error) {
      $("selectedPositionResult").textContent = "Ошибка расчёта: " + error.message;
    }
  }
  document.addEventListener("DOMContentLoaded", () => {
    $("refreshOrbitData")?.addEventListener("click", refresh);
    $("satSearch")?.addEventListener("input", render);
    $("satCategory")?.addEventListener("change", render);
    refresh();
  });
})();
