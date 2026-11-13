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
  const favoriteKey = "cyd-favorite-satellites-v1";
  const favoriteNames = {"25544":"МКС (ZARYA)","33591":"NOAA 19","43017":"AO-91 (Fox-1B)"};
  let favorites = new Set();
  try { favorites = new Set(JSON.parse(localStorage.getItem(favoriteKey) || "[]").map(String)); } catch (_) {}
  function saveFavorites(){ try { localStorage.setItem(favoriteKey, JSON.stringify([...favorites])); } catch (_) {} }
  function renderFavorites(){
    const root=$("favoriteSatellites"); if(!root)return;
    const ids=[...favorites];
    $("favoriteCount").textContent=ids.length+" ОБЪЕКТОВ";
    if(!ids.length){root.innerHTML='<span class="subtle">Нажмите ☆ у спутника, чтобы добавить его сюда</span>';return;}
    root.innerHTML=ids.map(id=>{const item=records.find(x=>String(x.NORAD_CAT_ID)===id);const name=item?item.OBJECT_NAME:(favoriteNames[id]||("NORAD "+id));return '<button type="button" class="btn favorite-chip" data-favorite-open="'+esc(id)+'" style="border-color:#8a713e;color:#ffe0a0">★ '+esc(name)+' <span style="opacity:.65">· '+esc(id)+'</span></button>';}).join("");
    root.querySelectorAll("[data-favorite-open]").forEach(button=>button.addEventListener("click",event=>{
       event.preventDefault();event.stopPropagation();
      const id=button.dataset.favoriteOpen, item=records.find(x=>String(x.NORAD_CAT_ID)===id);
      // Быстрый выбор из избранного синхронизирует каталог и выбранную цель наведения.
      const targetByNorad={"25544":"iss","33591":"noaa","43017":"ao91"};
      if(targetByNorad[id]&&typeof window.selectTargetFromFavorite==="function"){
        window.selectTargetFromFavorite(targetByNorad[id]);
      }
      if(item){
        selected=item;
        showDetails().catch(error=>{$("liveSatelliteDetails").textContent=error.message;});
        $("liveSatelliteDetails").scrollIntoView({behavior:"smooth",block:"nearest"});
      } else {
        $("satSearch").value=id;$("satCategory").value="all";render();
      }
      renderFavorites();
    }));
  }
  function toggleFavorite(id){id=String(id);if(favorites.has(id))favorites.delete(id);else favorites.add(id);saveFavorites();renderFavorites();renderFavoriteButtons();}
  function renderFavoriteButtons(){
    document.querySelectorAll("[data-favorite-id]").forEach(button=>{const on=favorites.has(button.dataset.favoriteId);button.textContent=on?"★ В избранном":"☆ В избранное";button.classList.toggle("btn-primary",on);});
    document.querySelectorAll("[data-live-favorite]").forEach(button=>{const on=favorites.has(button.dataset.liveFavorite);button.textContent=on?"★":"☆";button.setAttribute("aria-label",on?"Убрать из избранного":"Добавить в избранное");});
  }

  const groupVisibilityKey = "cyd-catalog-group-visibility-v1";
  const groupVisibility = Object.fromEntries(sources.map(source=>[source.group,true]));
  try {
    const saved=JSON.parse(localStorage.getItem(groupVisibilityKey)||"{}");
    sources.forEach(source=>{if(typeof saved[source.group]==="boolean")groupVisibility[source.group]=saved[source.group];});
  } catch (_) {}
  function saveGroupVisibility(){try{localStorage.setItem(groupVisibilityKey,JSON.stringify(groupVisibility));}catch(_){}}
  let catalogNotice = "";
  let selected = null;
  let trackingId = null;
  let trackingTimer = null;
  let satelliteLibReady = null;
  const $ = id => document.getElementById(id);
  const esc = value => String(value ?? "").replace(/[&<>"']/g, c => ({"&":"&amp;","<":"&lt;",">":"&gt;",'"':"&quot;","'":"&#39;"}[c]));
  const status = value => { if ($("orbitFeedStatus")) $("orbitFeedStatus").textContent = value; };
  async function loadSatelliteLib() {
    if (window.satellite && typeof window.satellite.json2satrec === "function") return window.satellite;
    if (!satelliteLibReady) satelliteLibReady = (async () => {
      const urls = [
        "https://cdn.jsdelivr.net/npm/satellite.js@6.0.1/+esm",
        "https://esm.sh/satellite.js@6.0.1?bundle",
        "https://unpkg.com/satellite.js@6.0.1/dist/satellite.es.js"
      ];
      const errors = [];
      for (const url of urls) {
        try {
          const imported = await import(url);
          const candidate = imported.default && typeof imported.default.json2satrec === "function"
            ? imported.default : imported;
          const required = ["json2satrec", "propagate", "gstime", "eciToEcf", "ecfToLookAngles"];
          const missing = required.filter(name => typeof candidate[name] !== "function");
          if (missing.length) throw new Error("нет функций: " + missing.join(", "));
          window.satellite = candidate;
          return candidate;
        } catch (error) {
          errors.push(url + ": " + (error && error.message ? error.message : String(error)));
        }
      }
      throw new Error("Не удалось загрузить satellite.js / SGP4. " + errors.join(" | "));
    })();
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
    window.cydSatelliteCatalogRecords=records;
    window.dispatchEvent(new CustomEvent('cyd-catalog-loaded',{detail:{records}}));
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

  // Images are served as static repository assets; the API remains a fallback
  // for objects not yet included in the checked-in image library.
  let satelliteImageManifest = {};
  const satelliteImageManifestReady = fetch("/satellite-images/manifest.json", {cache:"no-store"})
    .then(response => response.ok ? response.json() : null)
    .then(data => {
      satelliteImageManifest = data && data.images && typeof data.images === "object" ? data.images : {};
      if (records.length) render();
    })
    .catch(() => {});
  function satelliteImageUrl(norad) {
    const entry = satelliteImageManifest[String(norad)];
    return entry && entry.status === "available" && typeof entry.path === "string"
      ? entry.path
      : "/api/satellite-image?norad=" + encodeURIComponent(norad) + "&download=1";
  }
  function satelliteImageFallback(norad) {
    const entry = satelliteImageManifest[String(norad)];
    return entry && entry.status === "available" && typeof entry.path === "string"
      ? "/api/satellite-image?norad=" + encodeURIComponent(norad) + "&download=1"
      : "/satellite-placeholder.svg";
  }
  function satelliteImageError(norad) {
    const fallback = satelliteImageFallback(norad);
    return "if(this.dataset.fallback!=='1'){this.dataset.fallback='1';this.src='" + fallback + "';}else{this.onerror=null;this.src='/satellite-placeholder.svg';}";
  }

  const satelliteInfo = {
    "25544": {title:"Международная космическая станция · ISS",image:"/api/satellite-image?norad=25544&download=1",history:"Первый модуль выведен на орбиту в 1998 году. Постоянно обитаемый международный комплекс для научных экспериментов и наблюдения Земли.",tech:"Пилотируемый орбитальный комплекс на низкой околоземной орбите. Идентификатор каталога NORAD 25544. Массу не приводим: она меняется по мере стыковки кораблей и конфигурации станции.",frequencies:"Голос: приём 145.800 MHz FM; передача в ITU Region 1 — 145.200 MHz FM. Пакетный режим VHF: 145.825 MHz. Радиолюбительский FM-репитер: uplink 145.990 MHz (CTCSS 67 Hz) / downlink 437.800 MHz. Частоты и доступность радиостанций меняются; перед передачей сверяйтесь со статусом ARISS.",source:"https://www.ariss.org/contact-the-iss.html"},
    "43017": {title:"AO-91 · Fox-1B",image:"/api/satellite-image?norad=43017&download=1",history:"Любительский CubeSat AMSAT-NA, запущен в 2017 году для экспериментов и радиолюбительской связи.",tech:"CubeSat 1U с FM ретранслятором типа bent-pipe. NORAD 43017.",frequencies:"Uplink 435.250 MHz FM; downlink 145.960 MHz FM. По данным AMSAT работа ограничена состоянием аккумулятора и солнечным освещением; проверьте текущий статус.",source:"https://www.amsat.org/two-way-satellites/ao-91/"},
    "33591": {title:"NOAA-19 · метеоспутник",image:"/api/satellite-image?norad=33591&download=1",history:"Запущен 6 февраля 2009 года в составе программы NOAA POES для метеорологических наблюдений. NOAA вывела аппарат из эксплуатации 13 августа 2025 года.",tech:"Полярно-орбитальный метеоспутник серии POES. NORAD 33591. По данным NOAA, аппарат decommissioned; его не следует считать действующим источником метеоданных.",frequencies:"Историческая частота APT: 137.100 MHz; HRPT: 1698.0 MHz. NOAA-19 выведен из эксплуатации 13.08.2025 — не рассчитывайте на действующую передачу. Это справочные частоты, не подтверждение активности.",source:"https://www.ospo.noaa.gov/operations/poes/status.html"}
  };
  const categoryInfo = {
    stations:{history:"Орбитальные станции и транспортные корабли обслуживают экипажи, эксперименты и инфраструктуру на орбите.",tech:"Низкая околоземная орбита; конструкция и оборудование зависят от конкретного аппарата.",frequencies:"Частоты индивидуальны для каждого корабля и радиослужбы. Проверенная карточка частот для этого объекта пока не добавлена."},
    weather:{history:"Метеоспутники собирают данные об облачности, температуре поверхности и атмосфере.",tech:"Полярные и геостационарные платформы с радиометрами и каналами передачи метеоданных.",frequencies:"Частоты зависят от модели и режима (APT, HRPT, LRPT, HRIT). Не указываем типовую частоту без подтверждения для данного аппарата."},
    amateur:{history:"Радиолюбительские спутники предназначены для связи, экспериментов, телеметрии и обучения.",tech:"Часто это малые CubeSat с VHF/UHF радиолиниями, маяками или транспондерами.",frequencies:"Uplink/downlink индивидуальны и могут меняться. Проверенные значения для этого NORAD ID пока не добавлены."},
    gnss:{history:"Навигационные аппараты обеспечивают глобальное определение координат и точного времени.",tech:"Средняя околоземная орбита и навигационные сигналы в нескольких диапазонах L-band.",frequencies:"Частоты зависят от системы и блока спутника; требуется идентификация конкретного аппарата."},
    science:{history:"Научные аппараты исследуют Землю, Солнце, космос и физические процессы.",tech:"Полезная нагрузка, орбита и радиолинии определяются научной миссией.",frequencies:"Подтверждённые каналы для этого NORAD ID не добавлены."},
    cubesat:{history:"CubeSat — стандартизированный формат малых спутников для университетских, научных и коммерческих миссий.",tech:"Модульная платформа; базовый модуль 1U имеет размер около 10×10×10 см.",frequencies:"Радиочастоты индивидуальны для миссии. Без подтверждённой координации оператора не указываются."},
    visual:{history:"Объект включён в каталог наблюдаемых спутников проекта.",tech:"Орбитальные элементы загружаются из OMM/CelesTrak и рассчитываются методом SGP4.",frequencies:"Рабочие частоты зависят от назначения и оборудования. Для точных значений требуется идентификация радиосистемы."}
  };
  function ensureSatellitePreview(){
    if($("satelliteHoverPreview")) return;
    const css=document.createElement("style");
    css.textContent="#satelliteHoverPreview{position:fixed;inset:0;z-index:99999;display:flex;align-items:center;justify-content:center;padding:16px;background:rgba(2,8,16,.78);backdrop-filter:blur(8px);color:#edf6ff;font:13px/1.5 -apple-system,BlinkMacSystemFont,sans-serif}#satelliteHoverPreview[hidden]{display:none}#satelliteHelpCard{position:relative;width:min(620px,100%);max-height:88vh;overflow:auto;padding:16px;border:1px solid #52718c;border-radius:16px;background:#0c1624;box-shadow:0 18px 55px #0009}#satelliteHelpCard img{display:block;width:100%;height:220px;object-fit:contain;object-position:center;background:rgba(255,255,255,.06);border-radius:12px;margin:10px 0 12px}#satelliteHelpCard h3{font-size:16px;margin:0 0 7px;padding-right:90px}#satelliteHelpCard p{margin:7px 0}#satelliteHelpCard b,#satelliteHelpCard a{color:#8ed7ff}#satelliteHelpClose{position:absolute;right:14px;top:12px;border:1px solid #52718c;border-radius:8px;background:#102b43;color:#eaf7ff;padding:7px 12px;cursor:pointer}";
    document.head.appendChild(css);
    const box=document.createElement("aside");box.id="satelliteHoverPreview";box.hidden=true;box.setAttribute("role","dialog");box.setAttribute("aria-modal","true");box.setAttribute("aria-label","Справка о спутнике");
    box.addEventListener("click",event=>{if(event.target===box)hideSatellitePreview();});
    document.body.appendChild(box);
  }
  function hideSatellitePreview(){const box=$("satelliteHoverPreview");if(box)box.hidden=true;}
  function showSatellitePreview(item){
    ensureSatellitePreview();
    const box=$("satelliteHoverPreview"),info=satelliteInfo[String(item.NORAD_CAT_ID)]||{},generic=categoryInfo[item._category]||categoryInfo.visual;
    const imageUrl=satelliteImageUrl(item.NORAD_CAT_ID);
    const img='<img src="'+esc(imageUrl)+'" alt="Изображение '+esc(item.OBJECT_NAME)+'" loading="lazy" onerror="'+esc(satelliteImageError(item.NORAD_CAT_ID))+'">';
    box.innerHTML='<section id="satelliteHelpCard"><button type="button" id="satelliteHelpClose">Закрыть ✕</button>'+img+'<h3>'+esc(info.title||item.OBJECT_NAME)+'</h3>'+
      '<p><b>История:</b> '+esc(info.history||generic.history)+'</p>'+
      '<p><b>Технически:</b> '+esc(info.tech||generic.tech)+'</p>'+
      '<p><b>Рабочие частоты:</b> '+esc(info.frequencies||generic.frequencies)+'</p>'+
      '<p style="font-size:11px;color:#9eb1c5">Фотографии сохраняются в библиотеке проекта на GitHub; если фото аппарата нет в открытых источниках, показывается условная иллюстрация.</p>'+
      (info.source?'<p><a href="'+esc(info.source)+'" target="_blank" rel="noopener">Источник характеристик ↗</a></p>':'')+
      '<p style="font-size:11px;color:#9eb1c5">Справочная информация не гарантирует текущую работоспособность радиоканала. Частоты и статус проверяйте по ссылке на первичный источник перед использованием.</p></section>';
    box.hidden=false;
    $("satelliteHelpClose").addEventListener("click",hideSatellitePreview);
    $("satelliteHelpClose").focus();
  }
  document.addEventListener("keydown",event=>{if(event.key==="Escape")hideSatellitePreview();});
  let catalogSortToken = 0;
  async function rankByVisibility(items) {
    const token = ++catalogSortToken;
    const sat = await loadSatelliteLib();
    const latInput = Number($("observerLat").value), lonInput = Number($("observerLon").value);
    // Если координаты станции ещё не введены, используем координаты станции проекта.
    const observer = {
      latitude: Number.isFinite(latInput) && $("observerLat").value.trim() !== "" ? latInput : 56.8389,
      longitude: Number.isFinite(lonInput) && $("observerLon").value.trim() !== "" ? lonInput : 60.6057,
      altitude: Number($("observerAltitude").value || 0)
    };
    const threshold = 10, now = new Date(), horizonMs = 24 * 3600000, stepMs = 120000;
    const ranked = items.map(item => ({item, visible:false, nextPass:null, elevation:-90}));
    // Сначала определяем видимые спутники, затем ищем ближайшее пересечение порога 10°.
    for (const row of ranked) {
      try {
        const satrec = sat.json2satrec(row.item);
        const current = getTopocentric(sat, satrec, observer, now);
        if (!current) continue;
        row.elevation = current.elevation;
        row.visible = current.elevation >= threshold;
        if (row.visible) continue;
        let prevTime = now, prev = current;
        for (let ms = stepMs; ms <= horizonMs; ms += stepMs) {
          const time = new Date(now.getTime() + ms);
          const point = getTopocentric(sat, satrec, observer, time);
          if (point && prev.elevation < threshold && point.elevation >= threshold) {
            row.nextPass = interpolateCrossing(prevTime, prev.elevation, time, point.elevation, threshold);
            break;
          }
          if (point) { prev = point; prevTime = time; }
        }
      } catch (_) {}
    }
    if (token !== catalogSortToken) return items;
    ranked.sort((a,b) => {
      if (a.visible !== b.visible) return a.visible ? -1 : 1;
      if (a.visible && b.visible) return b.elevation - a.elevation;
      if (a.nextPass && b.nextPass) return a.nextPass - b.nextPass;
      if (a.nextPass) return -1;
      if (b.nextPass) return 1;
      return b.elevation - a.elevation;
    });
    return ranked.map(row => row);
  }
  async function render() {
    const query = $("satSearch").value.trim().toLowerCase();
    const filtered = records.filter(item => categoryMatch(item) &&
      (String(item.OBJECT_NAME).toLowerCase().includes(query) || String(item.NORAD_CAT_ID).includes(query)));
    const list = $("liveSatelliteList");
    list.innerHTML = catalogNotice + '<div class="note">Рассчитываем видимость и ближайшие пролёты…</div>';
    let ranked;
    try { ranked = await rankByVisibility(filtered.slice(0, 100)); }
    catch (_) { ranked = filtered.slice(0, 100).map(item => ({item,visible:false,nextPass:null,elevation:-90})); }
    if (!ranked.length) { list.innerHTML = catalogNotice + '<div class="note">По запросу ничего не найдено.</div>'; return; }
    const rowMarkup = row => {
      const item = row.item;
      const age = item.EPOCH ? (Date.now() - Date.parse(item.EPOCH)) / 3600000 : null;
      const ageLabel = age === null ? "эпоха не указана" : age < 0 ? "эпоха из будущего — проверьте источник" :
        age > 168 ? "устарело: " + (age / 24).toFixed(1) + " сут." : "эпоха " + age.toFixed(1) + " ч назад";
      const ageColor = age !== null && age > 168 ? "#ffb36b" : "#8da5bc";
      const passLabel = row.visible ? "● ВИДЕН СЕЙЧАС · " + row.elevation.toFixed(1) + "°" :
        row.nextPass ? "Пролёт: " + row.nextPass.toLocaleTimeString([], {hour:"2-digit",minute:"2-digit"}) :
        "Нет пролёта за 24 ч";
      const passColor = row.visible ? "#55e1a7" : row.nextPass ? "#8cddff" : "#8da5bc";
      const tracking = trackingId === String(item.NORAD_CAT_ID);
      return '<div class="object-row" style="width:100%;text-align:left;color:inherit"><button type="button" data-live-id="' + esc(item.NORAD_CAT_ID) + '" style="flex:1;min-width:0;text-align:left;background:none;border:0;color:inherit;cursor:pointer;padding:0"><b>' +
        esc(item.OBJECT_NAME) + '</b><small>NORAD ' + esc(item.NORAD_CAT_ID) + ' · ' + esc(item._categoryLabel || item._category) +
        ' · <span style="color:' + passColor + '">' + esc(passLabel) + '</span><br><span style="color:' + ageColor + '">' + esc(ageLabel) + '</span></small></button><button type="button" class="btn" data-live-help="' + esc(item.NORAD_CAT_ID) + '" style="padding:7px 10px;white-space:nowrap;font-size:12px">Справка</button><button type="button" class="btn" data-live-track="' + esc(item.NORAD_CAT_ID) + '" style="padding:7px 10px;white-space:nowrap;font-size:12px" aria-pressed="' + tracking + '">' + (tracking ? '● Слежение' : 'Следить') + '</button><button type="button" class="btn" data-live-favorite="' + esc(item.NORAD_CAT_ID) + '" style="padding:7px 11px;font-size:18px" aria-label="Добавить в избранное">☆</button></div>';
    };
    const groupSections = sources.map(source=>{
      const rows=ranked.filter(row=>row.item._category===source.group);
      if(!rows.length)return "";
      const enabled=groupVisibility[source.group]!==false;
      return '<section class="catalog-group" style="margin:10px 0;border:1px solid #29445d;border-radius:12px;overflow:hidden">' +
        '<label style="display:flex;align-items:center;gap:10px;padding:12px;background:#10263a;cursor:pointer;font-weight:700">' +
        '<input type="checkbox" data-group-toggle="'+source.group+'" '+(enabled?'checked':'')+' style="width:19px;height:19px;accent-color:#55c7ff">' +
        '<span style="flex:1">'+esc(source.label)+'</span><span class="subtle">'+rows.length+' объектов</span></label>' +
        (enabled?'<div style="padding:8px">'+rows.map(rowMarkup).join("")+'</div>':'')+'</section>';
    }).join("");
    const visibleGroupCount=sources.filter(source=>groupVisibility[source.group]!==false).length;
    list.innerHTML = catalogNotice +
      '<div class="note" style="margin-bottom:8px">Сначала отображаются видимые сейчас спутники, затем — по времени ближайшего пролёта. Отключение группы скрывает её в списке, но не исключает спутники из расчётов уведомлений о восходе.</div>' +
      '<div class="button-row" style="margin:8px 0"><button type="button" class="btn" id="catalogGroupsAll">Включить все группы</button><button type="button" class="btn" id="catalogGroupsNone">Отключить все группы</button><small class="subtle">'+visibleGroupCount+' из '+sources.length+' групп включено</small></div>' +
      groupSections;
    list.querySelectorAll("[data-group-toggle]").forEach(input=>input.addEventListener("change",()=>{
      groupVisibility[input.dataset.groupToggle]=input.checked;saveGroupVisibility();render();
    }));
    $("catalogGroupsAll")?.addEventListener("click",()=>{sources.forEach(source=>groupVisibility[source.group]=true);saveGroupVisibility();render();});
    $("catalogGroupsNone")?.addEventListener("click",()=>{sources.forEach(source=>groupVisibility[source.group]=false);saveGroupVisibility();render();});
    renderFavorites(); renderFavoriteButtons();
    list.querySelectorAll("[data-live-favorite]").forEach(button=>button.addEventListener("click",event=>{event.stopPropagation();toggleFavorite(button.dataset.liveFavorite);}));
    list.querySelectorAll("[data-live-track]").forEach(button=>button.addEventListener("click",event=>{event.preventDefault();event.stopPropagation();toggleCatalogTracking(button.dataset.liveTrack);}));
    list.querySelectorAll("[data-live-id]").forEach(button => {
      const item=records.find(entry=>String(entry.NORAD_CAT_ID)===button.dataset.liveId);
      button.addEventListener("click", () => {
        selected=item;
        showDetails().catch(error => { $("liveSatelliteDetails").textContent = error.message; });
      });
    });
    list.querySelectorAll("[data-live-help]").forEach(button=>button.addEventListener("click",event=>{
      event.preventDefault();event.stopPropagation();
      const item=records.find(entry=>String(entry.NORAD_CAT_ID)===button.dataset.liveHelp);
      if(item)showSatellitePreview(item);
    }));
  }
  function toggleCatalogTracking(id) {
    id=String(id);
    if(trackingId===id){
      trackingId=null;
      if(trackingTimer){clearInterval(trackingTimer);trackingTimer=null;}
      render();
      if($("liveSatelliteDetails")) $("liveSatelliteDetails").insertAdjacentHTML("afterbegin",'<div class="note" style="margin-bottom:8px">Слежение за объектом остановлено. Команды на антенну не отправлялись.</div>');
      return;
    }
    const item=records.find(entry=>String(entry.NORAD_CAT_ID)===id);
    if(!item)return;
    trackingId=id;
    selected=item;
    if(window.cydOrbitSceneReady){window.dispatchEvent(new CustomEvent("cyd-catalog-track",{detail:{record:item}}));}else{window.cydPendingCatalogTracks=window.cydPendingCatalogTracks||[];window.cydPendingCatalogTracks.push(item);}
    if(trackingTimer)clearInterval(trackingTimer);
    showDetails().then(()=>{
      const update=()=>{if(trackingId!==id||!selected||String(selected.NORAD_CAT_ID)!==id)return;calculatePosition();};
      update();
      trackingTimer=setInterval(update,5000);
    }).catch(error=>{$("liveSatelliteDetails").textContent=error.message;});
    render();
  }
  async function showDetails() {
    if (!selected) return;
    const epoch = selected.EPOCH ? new Date(selected.EPOCH).toLocaleString() : "не указана";
    const selectedImage=satelliteImageUrl(selected.NORAD_CAT_ID);
    const selectedImageEntry=satelliteImageManifest[String(selected.NORAD_CAT_ID)]||null;
    const imageAttribution=selectedImageEntry&&selectedImageEntry.sourcePage
      ? '<div style="margin:-5px 0 12px;color:#8faac0;font-size:10px;line-height:1.5">'+esc(selectedImageEntry.imageType==="render"?"Компьютерный рендер":"Фотография")+' · '+esc(selectedImageEntry.credit||selectedImageEntry.provider||"Источник")+'<br><a href="'+esc(selectedImageEntry.sourcePage)+'" target="_blank" rel="noopener" style="color:#67caff">Источник и условия использования ↗</a> · '+esc(selectedImageEntry.license||"Лицензия уточняется")+'</div>' : '';
    $("liveSatelliteDetails").innerHTML = '<div style="margin:10px 0 12px;border:1px solid #29445d;border-radius:12px;overflow:hidden;background:#091827"><img src="'+esc(selectedImage)+'" alt="Изображение '+esc(selected.OBJECT_NAME)+'" loading="lazy" style="display:block;width:100%;max-height:240px;object-fit:contain" onerror="'+esc(satelliteImageError(selected.NORAD_CAT_ID))+'"></div>'+imageAttribution+(trackingId===String(selected.NORAD_CAT_ID)?'<div class="note" style="margin-bottom:8px;color:#55e1a7">● Слежение активно · обновление положения каждые 5 секунд</div>':'') + '<b>' + esc(selected.OBJECT_NAME) + '</b> · NORAD ' + esc(selected.NORAD_CAT_ID) +
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
    document.querySelectorAll("[data-favorite-id]").forEach(button=>button.addEventListener("click",()=>toggleFavorite(button.dataset.favoriteId)));
    renderFavorites(); renderFavoriteButtons();
    $("refreshOrbitData")?.addEventListener("click", refresh);
    $("predictPasses")?.addEventListener("click", predictPasses);
    $("satSearch")?.addEventListener("input", render);
    $("satCategory")?.addEventListener("change", render);
    refresh();
  });
})();
