/* Live public satellite catalog. Orbit positions are computed locally; no hardware commands are sent. */
(() => {
  const sources = [
    { group: "stations", label: "Орбитальные станции" },
    { group: "weather", label: "Метеоспутники" },
    { group: "amateur", label: "Любительские" },
    { group: "gnss", label: "Навигационные GNSS" },
    { group: "science", label: "Научные" },
    { group: "cubesat", label: "CubeSat" },
    { group: "visual", label: "Наблюдаемые" }
  ];
  let records = [];
  let catalogNotice = "";
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
    status("Загрузка актуальных данных…");
    $("liveSatelliteList").innerHTML = '<div class="note">Получение орбитальных элементов CelesTrak…</div>';
    const results = await Promise.all(sources.map(async source => {
      const cacheKey = "cyd-omm-v1-" + source.group;
      let cached = null;
      try { cached = JSON.parse(localStorage.getItem(cacheKey) || "null"); } catch (_) {}
      // Always request the feed when the page opens or the user refreshes.
      // Keep local data only as an offline fallback; it must not block a fresh request.
      try {
        const response = await fetch("/api/satellites?group=" + encodeURIComponent(source.group), { cache: "no-store" });
        if (!response.ok) throw new Error("HTTP " + response.status + " · " + await response.text());
        const data = await response.json();
        if (!Array.isArray(data)) throw new Error("неверный формат OMM JSON");
        try { localStorage.setItem(cacheKey, JSON.stringify({savedAt: Date.now(), data})); } catch (_) {}
        return { source, data, error: null, cached: false, fetched: true };
      } catch (error) {
        return { source, data: cached && Array.isArray(cached.data) ? cached.data : [], error: error.message, stale: !!cached };
      }
    }));
    const byId = new Map();
    results.forEach(({source,data}) => data.forEach(item => {
      const id = String(item.NORAD_CAT_ID ?? "");
      const hasOmm = item.EPOCH && Number.isFinite(Number(item.MEAN_MOTION)) &&
        Number.isFinite(Number(item.INCLINATION)) && Number.isFinite(Number(item.ECCENTRICITY)) &&
        Number.isFinite(Number(item.RA_OF_ASC_NODE)) && Number.isFinite(Number(item.ARG_OF_PERICENTER)) &&
        Number.isFinite(Number(item.MEAN_ANOMALY));
      if (id && hasOmm) {
        byId.set(id, {...item, _category: source.group, _categoryLabel: source.label});
      }
    }));
    records = [...byId.values()];
    const failed = results.filter(result => result.error);
    const fetchedGroups = results.filter(result => result.fetched).length;
    const cachedGroups = results.filter(result => result.stale).length;
    const staleGroups = results.filter(result => result.stale).length;
    const epochs = records.map(item => Date.parse(item.EPOCH)).filter(Number.isFinite);
    const newest = epochs.length ? new Date(Math.max(...epochs)) : null;
    const oldest = epochs.length ? new Date(Math.min(...epochs)) : null;
    const ageHours = oldest ? (Date.now() - oldest.getTime()) / 3600000 : null;
    $("orbitUpdatedAt").textContent = "Загрузка: " + new Date().toLocaleString() +
      (newest ? " · свежайшая эпоха: " + newest.toLocaleString() : "");
    if (!records.length) {
      status("Источник недоступен");
      $("liveSatelliteList").innerHTML = '<div class="note">Не удалось получить каталог CelesTrak. Проверьте соединение и повторите обновление. ' +
        esc(failed.map(x => x.source.label + ": " + x.error).join("; ")) + '</div>';
      return;
    }
    status(records.length + " спутников · " + fetchedGroups + " категорий запрошено при открытии" + (cachedGroups ? " · резервный кэш" : ""));
    const freshness = ageHours === null ? "Эпоха TLE не определена" :
      ageHours > 168 ? "Внимание: есть элементы старше 7 суток — точность прогноза снижена" :
      ageHours > 72 ? "Некоторые орбитальные элементы старше 72 часов" : "Возраст орбитальных данных проверен";
    catalogNotice = '<div class="note" style="margin-bottom:8px">Источник: CelesTrak GP · ' + esc(freshness) +
      (oldest ? '<br>Самая старая эпоха в выборке: ' + esc(oldest.toLocaleString()) : '') +
      (fetchedGroups ? '<br>При открытии страницы отправлен запрос к источнику для ' + fetchedGroups + ' категорий' : '') +
      (cachedGroups ? '<br>Для недоступных категорий использованы сохранённые данные' : '') +
      (staleGroups ? '<br>Нет связи; использованы сохранённые данные (' + staleGroups + ' категорий)' : '') +
      (failed.length ? '<br>Ошибки источника: ' + esc(failed.map(x => x.source.label + " — " + x.error).join("; ")) : '') + '</div>';
    render();
  }
  function categoryMatch(item) {
    const category = $("satCategory").value;
    return category === "all" || item._category === category;
  }
  function render() {
    const query = $("satSearch").value.trim().toLowerCase();
    const visible = records.filter(item => categoryMatch(item) &&
      (String(item.OBJECT_NAME).toLowerCase().includes(query) || String(item.NORAD_CAT_ID).includes(query)));
    $("liveSatelliteList").innerHTML = catalogNotice + visible.slice(0, 100).map(item => {
      const age = item.EPOCH ? (Date.now() - Date.parse(item.EPOCH)) / 3600000 : null;
      const ageLabel = age === null ? "эпоха не указана" : age < 0 ? "эпоха из будущего — проверьте источник" :
        age > 168 ? "устарело: " + (age / 24).toFixed(1) + " сут." :
        "эпоха " + age.toFixed(1) + " ч назад";
      const ageColor = age !== null && age > 168 ? "#ffb36b" : "#8da5bc";
      return '<button type="button" class="object-row" data-live-id="' + esc(item.NORAD_CAT_ID) + '" style="width:100%;text-align:left;color:inherit;cursor:pointer"><span><b>' +
        esc(item.OBJECT_NAME) + '</b><small>NORAD ' + esc(item.NORAD_CAT_ID) + ' · ' + esc(item._categoryLabel || item._category) +
        ' · <span style="color:' + ageColor + '">' + esc(ageLabel) + '</span></small></span><strong>Выбрать ↗</strong></button>';
    }).join("") || '<div class="note">По запросу ничего не найдено.</div>';
    $("liveSatelliteList").querySelectorAll("[data-live-id]").forEach(button => button.addEventListener("click", () => {
      selected = records.find(item => String(item.NORAD_CAT_ID) === button.dataset.liveId);
      showDetails().catch(error => { $("liveSatelliteDetails").textContent = error.message; });
    }));
  }
  async function showDetails() {
    if (!selected) return;
    const epoch = selected.EPOCH ? new Date(selected.EPOCH).toLocaleString() : "не указана";
    $("liveSatelliteDetails").innerHTML = '<b>' + esc(selected.OBJECT_NAME) + '</b> · NORAD ' + esc(selected.NORAD_CAT_ID) +
      '<br>Эпоха орбитальных элементов (UTC): ' + esc(epoch) +
      '<br>Возраст эпохи: ' + esc(selected.EPOCH ? Math.max(0, (Date.now() - Date.parse(selected.EPOCH)) / 3600000).toFixed(1) + ' ч' : 'неизвестен') + '<br>Наклонение: ' + esc(selected.INCLINATION) + '° · Эксцентриситет: ' + esc(selected.ECCENTRICITY) +
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
      const satrec = sat.json2satrec(selected);
      const now = new Date();
      const positionAndVelocity = sat.propagate(satrec, now);
      if (!positionAndVelocity || !positionAndVelocity.position) throw new Error("Не удалось распространить орбиту для выбранного времени.");
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
  function observerSettings() {
    const lat = Number($("observerLat").value), lon = Number($("observerLon").value);
    const altitude = Number($("observerAltitude").value || 0);
    if (!Number.isFinite(lat) || !Number.isFinite(lon) || lat < -90 || lat > 90 || lon < -180 || lon > 180 ||
        !Number.isFinite(altitude) || altitude < -500 || altitude > 9000) {
      throw new Error("Введите корректные координаты станции и высоту от −500 до 9000 м.");
    }
    return { latitude: lat, longitude: lon, altitude };
  }
  function getTopocentric(sat, satrec, observer, date) {
    const pv = sat.propagate(satrec, date);
    if (!pv || !pv.position) return null;
    const ecf = sat.eciToEcf(pv.position, sat.gstime(date));
    const look = sat.ecfToLookAngles({
      latitude: sat.degreesToRadians(observer.latitude),
      longitude: sat.degreesToRadians(observer.longitude),
      height: observer.altitude / 1000
    }, ecf);
    return {
      elevation: sat.radiansToDegrees(look.elevation),
      azimuth: (sat.radiansToDegrees(look.azimuth) + 360) % 360
    };
  }
  function interpolateCrossing(t1, e1, t2, e2, threshold) {
    if (e2 === e1) return new Date(t2);
    const fraction = Math.max(0, Math.min(1, (threshold - e1) / (e2 - e1)));
    return new Date(t1.getTime() + fraction * (t2.getTime() - t1.getTime()));
  }
  async function predictPasses() {
    const output = $("passResults");
    const statusNode = $("passPredictionStatus");
    try {
      if (!selected) throw new Error("Сначала выберите спутник из каталога.");
      const observer = observerSettings();
      const sat = await loadSatelliteLib();
      const satrec = sat.json2satrec(selected);
      const threshold = Number($("passMinElevation").value);
      const horizonHours = Number($("passHorizon").value);
      const start = new Date();
      const end = new Date(start.getTime() + horizonHours * 3600000);
      const stepMs = 30000;
      let previousTime = start;
      let previous = getTopocentric(sat, satrec, observer, previousTime);
      if (!previous) throw new Error("SGP4 не смог рассчитать орбиту для текущей эпохи.");
      let active = previous.elevation >= threshold ? {
        aos: null, aosAz: previous.azimuth, maxEl: previous.elevation,
        maxTime: start, los: null, losAz: null, ongoing: true
      } : null;
      const passes = [];
      for (let ms = start.getTime() + stepMs; ms <= end.getTime(); ms += stepMs) {
        const time = new Date(Math.min(ms, end.getTime()));
        const current = getTopocentric(sat, satrec, observer, time);
        if (!current) { previousTime = time; previous = current; continue; }
        if (!active && previous && previous.elevation < threshold && current.elevation >= threshold) {
          active = {
            aos: interpolateCrossing(previousTime, previous.elevation, time, current.elevation, threshold),
            aosAz: current.azimuth, maxEl: current.elevation, maxTime: time,
            los: null, losAz: null, ongoing: false
          };
        }
        if (active && current.elevation > active.maxEl) {
          active.maxEl = current.elevation;
          active.maxTime = time;
        }
        if (active && previous && previous.elevation >= threshold && current.elevation < threshold) {
          active.los = interpolateCrossing(previousTime, previous.elevation, time, current.elevation, threshold);
          active.losAz = current.azimuth;
          passes.push(active);
          active = null;
        }
        previousTime = time;
        previous = current;
        if (ms >= end.getTime()) break;
      }
      if (active) { active.los = null; active.ongoing = true; passes.push(active); }
      if (!passes.length) {
        output.innerHTML = '<div class="note">За выбранный период пролётов выше ' + threshold +
          '° не найдено. Попробуйте увеличить горизонт прогноза или снизить порог.</div>';
      } else {
        const fmt = date => date ? date.toLocaleString([], {month:"2-digit",day:"2-digit",hour:"2-digit",minute:"2-digit"}) : "уже начался";
        output.innerHTML = passes.map((pass, index) => {
          const duration = pass.los && pass.aos ? Math.round((pass.los - pass.aos) / 60000) :
            pass.los ? Math.round((pass.los - start) / 60000) : null;
          return '<div class="object-row" style="display:block"><div style="display:flex;justify-content:space-between;gap:8px"><b>Пролёт ' + (index + 1) +
            (pass.ongoing && !pass.aos ? ' · спутник уже виден' : '') + '</b><strong>' + (duration !== null ? duration + ' мин' : 'в процессе / за пределами окна') +
            '</strong></div><small>Восход: ' + fmt(pass.aos) + ' · аз. ' + pass.aosAz.toFixed(1) + '°</small>' +
            '<small>Максимум: ' + fmt(pass.maxTime) + ' · ' + pass.maxEl.toFixed(1) + '°</small>' +
            '<small>Заход: ' + fmt(pass.los) + (pass.losAz !== null ? ' · аз. ' + pass.losAz.toFixed(1) + '°' : '') + '</small></div>';
        }).join("");
      }
      statusNode.textContent = passes.length + " пролётов · шаг расчёта 30 с";
    } catch (error) {
      output.innerHTML = '<div class="note">' + esc(error.message) + '</div>';
      statusNode.textContent = "Расчёт не выполнен";
    }
  }
  document.addEventListener("DOMContentLoaded", () => {
    $("refreshOrbitData")?.addEventListener("click", refresh);
    $("predictPasses")?.addEventListener("click", predictPasses);
    $("satSearch")?.addEventListener("input", render);
    $("satCategory")?.addEventListener("change", render);
    refresh();
  });
})();
