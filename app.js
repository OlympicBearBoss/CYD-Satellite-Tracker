const translations={
en:{
workspace:"WORKSPACE",nav_overview:"Overview",nav_radar:"Radar",nav_control:"Antenna control",nav_satellites:"Satellites",nav_telemetry:"Telemetry",nav_connection:"Connection",nav_journal:"Event log",nav_apps:"Apps",nav_settings:"Settings",demo_active:"Demo mode active",station:"GROUND STATION",offline_demo:"Simulation",demo_title:"Demonstration mode",demo_desc:"All data is synthetic. No real hardware is connected and no commands are sent to ESP32.",eyebrow:"CONTROL CENTER · CYD ESP32",welcome:"Welcome to the station",welcome_desc:"Satellite monitoring, antenna position and equipment status.",open_control:"Open controls ↗",earth_view:"Orbital overview",orbit_demo:"Schematic orbit visualization",selected_sat:"Selected satellite",tracking_demo:"DEMO",azimuth:"AZIMUTH",elevation:"ELEVATION",next_pass:"NEXT PASS",in_12:"In 12 minutes",max_elev:"Maximum elevation: 78.4°",all_satellites:"Satellite catalog →",axis_az:"AZIMUTH AXIS",axis_el:"ELEVATION AXIS",motor_current:"MOTOR CURRENT",supply_voltage:"SUPPLY VOLTAGE",demo_value:"Synthetic value",nominal_demo:"Nominal · demo",system_health:"System health",details:"Details →",memory:"Memory",recent_events:"Recent events",all_events:"Full log →",event_track:"ISS tracking started",event_sat:"NOAA 19 satellite detected",event_wifi:"Demo connection active",radar_eyebrow:"OBSERVATION",radar_heading:"Satellite radar",radar_desc:"Schematic object positions relative to the ground station.",sky_radar:"Sky view",object_list:"Tracked objects",radar_note:"Demo radar: positions are not calculated from current TLE data and do not represent the real sky.",rotator_eyebrow:"DUAL-AXIS ROTATOR",control_heading:"Antenna control",control_desc:"Set azimuth and elevation axis positions.",simulation_only:"SIMULATION ONLY",antenna_model:"Antenna model",current_position:"CURRENT POSITION",mechanic_note:"Illustrative model. Geometry is not a mechanical drawing.",axis_settings:"Axis settings",az_axis:"Azimuth · AZ",el_axis:"Elevation · EL",target_for_control:"Target object",center_axes:"Center axes",track_target:"Point to target",stop_motion:"■ Stop",sim_ready:"Simulation ready",catalog_eyebrow:"OBJECT LIBRARY",catalog_heading:"Satellite catalog",catalog_desc:"Sample objects and reference radio frequencies.",station_type:"Station",weather_type:"Weather",amateur_type:"Amateur",downlink:"Receive",uplink:"Transmit",iss_desc:"International Space Station in low Earth orbit.",noaa_desc:"POES weather satellite for Earth observation.",ao_desc:"Amateur radio satellite with a communications repeater.",select_target:"Select object",freq_warning:"Frequencies are shown for demonstration and may be incomplete or outdated. Check current sources and amateur radio regulations before operating.",telemetry_eyebrow:"EQUIPMENT MONITORING",telemetry_heading:"Telemetry & diagnostics",telemetry_desc:"Example readings for power, drives and computing module.",voltage:"SUPPLY VOLTAGE",az_current:"AZ MOTOR CURRENT",el_current:"EL MOTOR CURRENT",temperature:"TEMPERATURE",voltage_ok:"Within demo reference range",temp_demo:"Simulated sensor",compute_load:"Controller load",motor_status:"Motor drivers",az_driver:"Azimuth drive",el_driver:"Elevation drive",normal:"NORMAL",telemetry_note:"All telemetry values are test data and are not read from a real CYD or motor drivers.",network_eyebrow:"NETWORK INTERFACE",connection_heading:"Connect to CYD",connection_desc:"Mock network connection panel for the ESP32 controller.",cyd_desc:"2.8″ color display · Wi-Fi · ESP32",demo_connected:"Demo connection",network_settings:"Network parameters",protocol:"Protocol",api_note:"* Proposed architecture only. The API and real network communication are not implemented yet.",test_connection:"Test demo connection",connection_arch:"Proposed connection architecture",client_app:"Mobile application",controller_fw:"Firmware and API",motor_driver:"Motor drivers",events_eyebrow:"SYSTEM EVENTS",journal_heading:"Event log",journal_desc:"History of actions in the demo interface.",clear_log:"Clear log",event_history:"Event history",mobile_eyebrow:"MOBILE ECOSYSTEM",apps_heading:"CYD Tracker apps",apps_desc:"A unified interface for station control from different devices.",one_app:"One station.<br>Any device.",app_copy:"The planned app will connect to CYD over Wi-Fi, select satellites, display telemetry and control the rotator.",app_feature1:"Wi-Fi connection to ESP32",app_feature2:"Satellite radar and catalog",app_feature3:"Manual control and tracking",app_feature4:"System status and event log",design_stage:"Design stage",ios_status:"iOS · Planned",ipad_status:"iPadOS · Planned",android_status:"Android · Planned",apps_note:"This is an app concept screen. Mobile builds and connection to a real controller are not available yet.",preferences:"PREFERENCES",settings_heading:"Interface settings",settings_desc:"Language and demo panel preferences.",language_settings:"Interface language",language_desc:"Choose the language for all dashboard sections.",demo_settings:"Demonstration mode",synthetic_data:"Synthetic data",synthetic_desc:"Use test readings and satellite positions.",safe_mode:"Safe mode",safe_desc:"Do not send commands to real hardware.",about_project:"About the project",about_desc:"Satellite ground station interface based on ESP32 CYD.",footer_note:"Demo interface · Not connected to real hardware",connection_test_ok:"Demo check complete: sample connection parameters are displayed.",log_cleared:"Event log cleared.",target_selected:"Selected target: ",stopped:"Demo motion stopped.",centered:"Demo axes centered.",tracking:"Demo target selected: "
},
ru:{}
};
const objectData={iss:{name:"МКС (ZARYA)",norad:"NORAD 25544 · НОО",az:132.4,el:67.8},noaa:{name:"NOAA 19",norad:"NORAD 33591 · МЕТЕОСПУТНИК",az:218.6,el:34.2},ao91:{name:"AO-91 (Fox-1B)",norad:"NORAD 43017 · ЛЮБИТЕЛЬСКИЙ",az:74.2,el:21.5,real:false},starlink:{name:"Starlink",norad:"ГРУППА · LEO",az:305,el:18,real:false}};
let lang="ru",selected="iss",logItems=[
["19:42:15","INFO","Сопровождение МКС запущено","ISS tracking started"],
["19:40:32","SCAN","Обнаружен спутник NOAA 19","NOAA 19 satellite detected"],
["19:38:20","NET","Демо-соединение активно","Demo connection active"],
["19:35:10","BOOT","Демонстрационная система запущена","Demo system started"],
["19:32:17","WIFI","Тестовый Wi-Fi сигнал стабилен","Demo Wi-Fi signal stable"]
];
const $=id=>document.getElementById(id);
document.querySelectorAll("[data-i18n]").forEach(el=>{translations.ru[el.dataset.i18n]=el.innerHTML;});
Object.assign(translations.ru,{centered:"Оси установлены в центр (демо).",stopped:"Движение остановлено (демо).",tracking:"Демонстрационное наведение на цель: ",connection_test_ok:"Проверка демо завершена: отображаются тестовые параметры подключения.",});
function t(key){return translations[lang][key]||translations.ru[key]||key}
function setLanguage(next){lang=next;document.documentElement.lang=lang;document.querySelectorAll("[data-i18n]").forEach(el=>{const val=t(el.dataset.i18n);if(val.includes("<br>"))el.innerHTML=val;else el.textContent=val});$("language").textContent=lang==="ru"?"EN":"RU";document.querySelectorAll("[data-lang]").forEach(b=>b.classList.toggle("selected",b.dataset.lang===lang));const active=document.querySelector(".view.active");if(active){const key=active.id.replace("view-","");const nav=document.querySelector('[data-view="'+key+'"]');$("crumbTitle").textContent=nav?nav.querySelector("i").textContent:"";}renderLog();renderRadarObjects();renderTarget();}
function go(view){document.querySelectorAll(".view").forEach(v=>v.classList.toggle("active",v.id==="view-"+view));document.querySelectorAll(".nav-item").forEach(b=>b.classList.toggle("active",b.dataset.view===view));const nav=document.querySelector('[data-view="'+view+'"]');$("crumbTitle").textContent=nav?nav.querySelector("i").textContent:"";window.scrollTo({top:0,behavior:"smooth"});if(view==="connection"&&typeof refreshLiveDevice==="function")refreshLiveDevice();}
document.querySelectorAll("[data-view]").forEach(b=>b.addEventListener("click",()=>go(b.dataset.view)));
document.querySelectorAll("[data-go]").forEach(b=>b.addEventListener("click",()=>go(b.dataset.go)));
$("language").addEventListener("click",()=>setLanguage(lang==="ru"?"en":"ru"));
document.querySelectorAll("[data-lang]").forEach(b=>b.addEventListener("click",()=>setLanguage(b.dataset.lang)));
function setAxes(az,el){az=Number(az);el=Number(el);$("azRange").value=az;$("elRange").value=el;const a=az.toFixed(1)+"°",e=el.toFixed(1)+"°";$("azOut").textContent=a;$("elOut").textContent=e;$("overviewAz").textContent=a;$("overviewEl").textContent=e;$("kpiAz").textContent=a;$("kpiEl").textContent=e;$("dishAz").textContent=a+" AZ";$("dishEl").textContent=e+" EL";}
$("azRange").addEventListener("input",()=>setAxes($("azRange").value,$("elRange").value));
$("elRange").addEventListener("input",()=>setAxes($("azRange").value,$("elRange").value));
function renderTarget(){const o=objectData[selected];$("selectedName").textContent=o.name;$("selectedNorad").textContent=lang==="en"?o.norad.replace("НОО","LEO").replace("МЕТЕОСПУТНИК","WEATHER SATELLITE").replace("ЛЮБИТЕЛЬСКИЙ","AMATEUR"):o.norad;}
function selectTarget(id){selected=id;const o=objectData[id];renderTarget();setAxes(o.az,o.el);$("targetSelect").value=id;addLog("SELECT",lang==="ru"?"Выбран объект "+o.name:"Selected target "+o.name);go("control");}
document.querySelectorAll("[data-select]").forEach(b=>b.addEventListener("click",()=>selectTarget(b.dataset.select)));
$("targetSelect").addEventListener("change",e=>{selected=e.target.value;renderTarget();const o=objectData[selected];setAxes(o.az,o.el);});
$("centerBtn").addEventListener("click",()=>{setAxes(180,45);if(window.cydConnected){sendCydCommand({command:"set_target",azimuth_deg:180,elevation_deg:45});return;}$("controlStatus").textContent=t("centered");addLog("CTRL",t("centered"));});
$("trackBtn").addEventListener("click",()=>{const o=objectData[selected];if(window.cydConnected){const match=String(o.norad).match(/\d+/);if(!match){$("controlStatus").textContent="Не удалось определить NORAD ID.";return;}sendCydCommand({command:"follow_satellite",norad_id:Number(match[0]),name:o.name});return;}setAxes(o.az,o.el);$("controlStatus").textContent=t("tracking")+o.name;addLog("CTRL",t("tracking")+o.name);});
$("stopBtn").addEventListener("click",()=>{if(window.cydConnected){sendCydCommand({command:"stop"});return;}$("controlStatus").textContent=t("stopped");addLog("STOP",t("stopped"));});
async function refreshLiveDevice(){
 const result=$("connectionResult");if(result)result.textContent="Запрос данных…";
 try{
  const response=await fetch("/api/devices/telemetry",{credentials:"same-origin",cache:"no-store"});
  const data=await response.json().catch(()=>({}));if(!response.ok)throw Error(data.error||"Не удалось получить состояние");
  const device=(data.devices||[]).find(d=>d.status==="online")||(data.devices||[])[0];
  if(!device){$("liveDeviceBadge").querySelector("span").textContent="Нет привязанной платы";$("liveDeviceInfo").textContent="Создайте код в профиле → Мои устройства.";["liveDeviceIp","liveDeviceRssi","liveDeviceGps","liveDeviceCoords","liveDeviceSeen"].forEach(id=>$(id).textContent="—");if(result)result.textContent="Сервер доступен. Устройство пока не передаёт телеметрию.";return;}
  const tdata=device.telemetry||{},online=device.status==="online";
  $("liveDeviceBadge").querySelector("span").textContent=online?"Устройство в сети":"Устройство не в сети";
  $("liveDeviceInfo").textContent=device.device_name+" · "+(tdata.firmware||"прошивка не указана");
  $("liveDeviceIp").textContent=tdata.ip||"—";$("liveDeviceRssi").textContent=Number.isFinite(tdata.wifi_rssi)?tdata.wifi_rssi+" dBm":"—";
  $("liveDeviceGps").textContent=tdata.gps_valid?"FIX · "+(tdata.gps_satellites??"—")+" спутн.":"Нет FIX";
  $("liveDeviceCoords").textContent=tdata.gps_valid&&Number.isFinite(tdata.latitude)&&Number.isFinite(tdata.longitude)?tdata.latitude.toFixed(5)+", "+tdata.longitude.toFixed(5):"—";
  $("liveDeviceSeen").textContent=device.last_seen_at?new Date(device.last_seen_at*1000).toLocaleString():"—";
  if(result)result.textContent=online?"Телеметрия получена с устройства.":"Последняя связь: "+(device.last_seen_at?new Date(device.last_seen_at*1000).toLocaleString():"нет данных");
 }catch(e){if(result)result.textContent=e.message;}
}
$("testConnection").addEventListener("click",refreshLiveDevice);
$("openDevicePairing").addEventListener("click",()=>{const trigger=$("accountProfileOpen");if(trigger)trigger.click();else $("connectionResult").textContent="Сначала войдите в аккаунт.";});

function renderRadarObjects(){
 const list=$("radarObjects");
 if(list)list.innerHTML=Object.entries(objectData).map(([id,o])=>'<div class="object-row"><div><b><i class="radar-object-dot '+id+'"></i>'+o.name+'</b><small>'+o.norad+' · '+(o.real?(o.el>0?"ВИДЕН":"НИЖЕ ГОРИЗОНТА"):"ДЕМО")+'</small></div><strong>'+o.az.toFixed(1)+'° / '+o.el.toFixed(1)+'°</strong></div>').join("");
 const rotor=$("radarRotor"),cardinals=$("radarCardinals");if(!rotor||!cardinals)return;
 rotor.querySelectorAll(".radar-dot").forEach(n=>n.remove());const heading=Number.isFinite(window.demoDeviceHeading)?window.demoDeviceHeading:0;
 [["N","С"],["E","В"],["S","Ю"],["W","З"]].forEach(([key,label],i)=>{const angle=(i*90-heading)*Math.PI/180,r=.465,el=cardinals.querySelector('[data-dir="'+key+'"]');el.textContent=label;el.style.left=(50+Math.sin(angle)*r*100)+"%";el.style.top=(50-Math.cos(angle)*r*100)+"%";});
 Object.entries(objectData).forEach(([id,o])=>{if(o.el<=0)return;const rel=o.az*Math.PI/180,radius=.88-(Math.min(90,o.el)/90)*.65,dot=document.createElement("span");dot.className="radar-dot "+id;dot.style.left=(50+Math.sin(rel)*radius*45)+"%";dot.style.top=(50-Math.cos(rel)*radius*45)+"%";const label=document.createElement("span");label.className="radar-dot-label";label.textContent=id==="iss"?"МКС":id==="noaa"?"NOAA 19":id==="ao91"?"AO-91":"STARLINK";dot.appendChild(label);rotor.appendChild(dot);});
}
window.addEventListener("cyd-orbit-update",()=>{const positions=window.cydRadarPositions||{};Object.entries(positions).forEach(([id,p])=>{if(!objectData[id])return;objectData[id].az=p.az;objectData[id].el=p.el;objectData[id].real=true;});renderRadarObjects();});
const compassNames=["СЕВЕР","СЕВЕРО-ВОСТОК","ВОСТОК","ЮГО-ВОСТОК","ЮГ","ЮГО-ЗАПАД","ЗАПАД","СЕВЕРО-ЗАПАД"];
let deviceHeading=null,geoWatchId=null;
function setDeviceHeading(value){
 if(!Number.isFinite(value))return;deviceHeading=((value%360)+360)%360;window.demoDeviceHeading=deviceHeading;
 $("compassRose").style.transform="rotate("+(-deviceHeading)+"deg)";
 $("compassRose").querySelectorAll(".compass-label").forEach(el=>el.style.transform="translate(-50%,-50%) rotate("+deviceHeading+"deg)");
 const dir=compassNames[Math.round(deviceHeading/45)%8];$("radarHeading").textContent=deviceHeading.toFixed(1)+"°";$("compassDirection").textContent=dir;renderRadarObjects();
}
function startDeviceOrientation(){
 const onOrientation=e=>{
  let heading=null;
  if(Number.isFinite(e.webkitCompassHeading))heading=e.webkitCompassHeading;
  else if(e.absolute===true&&Number.isFinite(e.alpha))heading=(360-e.alpha)%360;
  if(heading!==null){setDeviceHeading(heading);$("deviceSensorNote").textContent="Компас получает курс от датчика ориентации устройства. Углы объектов на радаре остаются демонстрационными.";}
 };
 window.addEventListener("deviceorientationabsolute",onOrientation,true);
 window.addEventListener("deviceorientation",onOrientation,true);
 if(typeof DeviceOrientationEvent!=="undefined"&&typeof DeviceOrientationEvent.requestPermission==="function"){
  DeviceOrientationEvent.requestPermission().then(state=>{if(state!=="granted")throw Error("Доступ к компасу не разрешён");}).catch(err=>{$("deviceSensorNote").textContent="Компас недоступен: "+err.message+". Проверьте разрешение датчиков в настройках браузера.";});
 }
}
function startDeviceLocation(){
 const status=$("gpsStatus"),note=$("deviceSensorNote");
 if(!window.isSecureContext){status.textContent="НУЖЕН HTTPS";note.textContent="Геолокация разрешена браузерами только в защищённом контексте. Откройте сайт по адресу https://cyd-satellite-tracker.pages.dev/.";return;}
 if(!navigator.geolocation){status.textContent="GPS НЕДОСТУПЕН";note.textContent="Браузер не предоставляет Geolocation API. Откройте сайт в Safari или Chrome на устройстве с включённой геолокацией.";return;}
 if(geoWatchId!==null){navigator.geolocation.clearWatch(geoWatchId);geoWatchId=null;}
 status.innerHTML="<i></i>ЗАПРОС GPS";
 note.textContent="Ожидаю разрешение браузера и координаты устройства. На телефоне включите системную геолокацию и разрешите доступ этому сайту.";
 const onPosition=pos=>{
  const c=pos.coords,lat=c.latitude,lon=c.longitude;window.cydObserverLocation={latitude:lat,longitude:lon,height:Number.isFinite(c.altitude)?c.altitude/1000:0};window.dispatchEvent(new Event("cyd-observer-change"));
  $("gpsLat").textContent=Math.abs(lat).toFixed(5)+"° "+(lat>=0?"N":"S");
  $("gpsLon").textContent=Math.abs(lon).toFixed(5)+"° "+(lon>=0?"E":"W");
  $("gpsAlt").textContent=Number.isFinite(c.altitude)?Math.round(c.altitude)+" м":"—";
  $("gpsAccuracy").textContent="±"+Math.round(c.accuracy)+" м";
  $("gpsTime").textContent=new Date(pos.timestamp).toLocaleTimeString("ru-RU");
  status.innerHTML="<i></i>GPS УСТРОЙСТВА";
  note.textContent="Координаты получены от службы геолокации устройства. Точность зависит от GPS, Wi-Fi, мобильной сети и разрешений ОС.";
 };
 const onError=err=>{
  status.textContent=err.code===1?"ДОСТУП ЗАПРЕЩЁН":err.code===2?"НЕТ СИГНАЛА":"ТАЙМ-АУТ";
  const details=err.code===1?"Браузер или система запретили геолокацию. В настройках устройства откройте разрешения сайта и включите «Местоположение»."
   :err.code===2?"Устройство не смогло определить координаты. Включите службы геолокации, выйдите к открытому небу и повторите запрос."
   :"Поиск координат занял слишком много времени. Проверьте системную геолокацию и повторите запрос.";
  note.textContent=details+" Если ранее выбрали «Запретить», измените разрешение сайта в настройках браузера.";
 };
 try{
  geoWatchId=navigator.geolocation.watchPosition(onPosition,onError,{enableHighAccuracy:true,maximumAge:0,timeout:30000});
 }catch(err){status.textContent="ОШИБКА GPS";note.textContent="Не удалось запустить геолокацию: "+err.message;}
 if(navigator.permissions&&navigator.permissions.query){
  navigator.permissions.query({name:"geolocation"}).then(p=>{
   if(p.state==="denied"){status.textContent="ДОСТУП ЗАПРЕЩЁН";note.textContent="Доступ к местоположению заблокирован в настройках браузера. Разрешите геолокацию для cyd-satellite-tracker.pages.dev и нажмите кнопку ещё раз.";}
  }).catch(()=>{});
 }
}
$("enableDeviceSensors").addEventListener("click",()=>{startDeviceLocation();startDeviceOrientation();});
function updateDemoNavigation(){if(deviceHeading===null){$("radarHeading").textContent="—";$("compassDirection").textContent="Ожидание датчика ориентации";}}
function addLog(code,message){const d=new Date();logItems.unshift([d.toLocaleTimeString("ru-RU"),code,message,message]);logItems=logItems.slice(0,30);renderLog();}
function renderLog(){$("fullLog").innerHTML=logItems.map(row=>'<div class="log-row"><i class="event-dot '+(row[1]==="STOP"?"orange":row[1]==="NET"?"green":"blue")+'"></i><time>'+row[0]+'</time><code>'+row[1]+'</code><p>'+(lang==="en"?row[3]:row[2])+'</p></div>').join("");$("logCount").textContent=logItems.length+" EVENTS";}
$("clearLog").addEventListener("click",()=>{logItems=[];renderLog();});
function tick(){const d=new Date();$("clock").textContent=d.toLocaleTimeString(lang==="ru"?"ru-RU":"en-GB");$("today").textContent=d.toLocaleDateString(lang==="ru"?"ru-RU":"en-GB",{day:"2-digit",month:"long",year:"numeric"});}
tick();setInterval(tick,1000);setAxes(objectData.iss.az,objectData.iss.el);updateDemoNavigation();renderRadarObjects();renderLog();setLanguage("ru");

// ── Direct local CYD connection ───────────────────────────────────────────
window.cydConnected = false;
let cydBaseUrl = "";
let cydPollTimer = null;
let cydEverConnected = false;
const cydNoData = () => lang === "en" ? "Not connected" : "Не подключен";
const cydText = (ru,en) => lang === "en" ? en : ru;
function cydSet(id,value){const el=$(id);if(el)el.textContent=value;}
function cydSetPill(id,connected){
 const el=$(id);if(!el)return;
 const label=el.querySelector("span");if(label)label.textContent=connected?cydText("Подключен","Connected"):cydNoData();
 el.style.color=connected?"#7fe1b7":"#ffb4bd";
}
function cydSetUnavailable(){
 const missing=cydNoData();
 ["kpiAz","kpiEl","kpiMotorCurrent","kpiSupplyVoltage","voltageValue","voltageGauge","azCurrentValue","azCurrentGauge","elCurrentValue","elCurrentGauge","temperatureValue","temperatureGauge","healthCpu","healthMemory"].forEach(id=>cydSet(id,missing));
 ["voltageHint","azCurrentHint","elCurrentHint","temperatureHint","kpiMotorCurrentHint","kpiSupplyVoltageHint","healthCpuBar","healthMemoryBar"].forEach(id=>{const el=$(id);if(el){if(el.tagName==="I")el.style.width="0%";else el.textContent=missing;}});
 cydSet("overviewAz",missing);cydSet("overviewEl",missing);
 cydSet("healthWifiRssi",missing);
 const wifiBar=$("healthWifiBar");if(wifiBar)wifiBar.style.width="0%";
 ["liveDeviceGps","liveDeviceAngle","liveDeviceCompass","liveDeviceDrivers","liveDeviceLimits","liveDeviceCoords","liveDeviceRssi"].forEach(id=>cydSet(id,missing));
 cydSetPill("azDriverStatus",false);cydSetPill("elDriverStatus",false);
 cydSet("gpsLat",missing);cydSet("gpsLon",missing);cydSet("gpsAlt",missing);cydSet("gpsAccuracy",missing);cydSet("gpsSatCount",missing);cydSet("gpsTime",missing);
 cydSet("radarHeading",missing);cydSet("compassDirection",missing);
 const gpsStatus=$("gpsStatus");if(gpsStatus)gpsStatus.textContent=missing.toUpperCase();
}
function setCydMode(mode){
 const demo=$("demoWarning");if(demo)demo.hidden=mode!=="demo";
 const connected=mode==="connected";
 const label=connected?cydText("CYD ПОДКЛЮЧЕНО","CYD CONNECTED"):mode==="offline"?cydText("CYD НЕТ СВЯЗИ","CYD OFFLINE"):cydText("Симуляция","Simulation");
 cydSet("connectionModeLabel",label);
 cydSet("sidebarModeLabel",connected?cydText("Реальное устройство подключено","Live device connected"):mode==="offline"?cydText("Связь с CYD потеряна","CYD connection lost"):cydText("Демо-режим активен","Demo mode active"));
 const chip=document.querySelector(".connection-chip i");if(chip)chip.style.background=connected?"var(--green)":mode==="offline"?"var(--red)":"var(--green)";
 cydSet("telemetryModeBadge",connected?cydText("РЕАЛЬНЫЕ ДАННЫЕ","LIVE DATA"):mode==="offline"?cydText("НЕТ СВЯЗИ","OFFLINE"):"DEMO DATA");
 const controlBadge=document.querySelector('#view-control .page-heading .status-pill span');
 if(controlBadge)controlBadge.textContent=connected?cydText("CYD · ПРИВОДЫ ЗАБЛОКИРОВАНЫ","CYD · DRIVES LOCKED"):mode==="offline"?cydText("CYD НЕТ СВЯЗИ","CYD OFFLINE"):t("simulation_only");
 const footer=document.querySelector('footer [data-i18n="footer_note"]');
 if(footer)footer.textContent=connected?cydText("Подключено к реальному CYD","Connected to real CYD"):mode==="offline"?cydText("CYD недоступен · реальные показания не поступают","CYD offline · no live readings") : t("footer_note");
}
function normalizeCydUrl(value){
 let raw=String(value||"").trim();if(!raw)throw Error(cydText("Укажите IP-адрес CYD, например http://192.168.1.50","Enter the CYD address, e.g. http://192.168.1.50"));
 if(!/^https?:\/\//i.test(raw))raw="http://"+raw;
 let u;try{u=new URL(raw);}catch{throw Error(cydText("Некорректный адрес CYD","Invalid CYD address"));}
 if(u.protocol!=="http:"&&u.protocol!=="https:")throw Error(cydText("Поддерживается HTTP/HTTPS адрес устройства","Use an HTTP/HTTPS device address"));
 if(u.username||u.password||u.pathname!=="/"&&u.pathname!=="")throw Error(cydText("Укажите только адрес устройства без пути и пароля","Enter only the device address, without path or password"));
 return u.origin;
}
function showCydAddress(base){
 ["cydDeviceAddress","cydConnectionAddress"].forEach(id=>{const el=$(id);if(el)el.value=base;});
}
async function requestCydStatus(base){
 const controller=new AbortController(),timeout=setTimeout(()=>controller.abort(),3500);
 try{
  const response=await fetch(base+"/api/v1/status",{method:"GET",mode:"cors",credentials:"omit",cache:"no-store",headers:{Accept:"application/json"},signal:controller.signal});
  if(!response.ok)throw Error("HTTP "+response.status);
  const data=await response.json();
  if(data.ok!==true||data.device!=="CYD Satellite Tracker"||!data.hardware_id)throw Error(cydText("Ответ не похож на API CYD","Response is not a CYD API"));
  return data;
 }finally{clearTimeout(timeout);}
}
function updateCydSensorReadings(data){
 const hw=data.hardware||{},gpsReady=hw.gps_connected===true&&data.gps_valid===true;
 const noData=cydNoData();
 cydSet("liveDeviceIp",data.ip||noData);
 cydSet("liveDeviceRssi",Number.isFinite(data.wifi_rssi)?data.wifi_rssi+" dBm":noData);
 cydSet("liveDeviceGps",gpsReady?cydText("FIX · ","FIX · ")+(data.gps_satellites??"—")+" "+cydText("спутн.","satellites"):noData);
 cydSet("liveDeviceAngle",hw.angle_sensor_connected===true?cydText("Подключен · нет показаний","Connected · no reading"):noData);
 cydSet("liveDeviceCompass",hw.compass_connected===true&&Number.isFinite(data.compass_heading_deg)?data.compass_heading_deg.toFixed(1)+"°":noData);
 cydSet("liveDeviceDrivers",hw.motor_drivers_connected===true?cydText("Подключены","Connected"):noData);
 cydSet("liveDeviceLimits",hw.limit_switches_connected===true?cydText("Подключены","Connected"):noData);
 cydSet("liveDeviceCoords",gpsReady&&Number.isFinite(data.latitude)&&Number.isFinite(data.longitude)?data.latitude.toFixed(5)+", "+data.longitude.toFixed(5):noData);
 cydSet("liveDeviceSeen",new Date().toLocaleString());
 cydSet("liveDeviceInfo",(data.device||"CYD")+" · "+(data.firmware||"Firmware"));
 cydSet("liveDeviceBadge",cydText("CYD подключён по Wi-Fi","CYD connected over Wi-Fi"));
 const badge=$("liveDeviceBadge");if(badge){const span=badge.querySelector("span");if(span)span.textContent=cydText("CYD подключён по Wi-Fi","CYD connected over Wi-Fi");}
 cydSet("healthWifiRssi",Number.isFinite(data.wifi_rssi)?data.wifi_rssi+" dBm":noData);
 const wifiBar=$("healthWifiBar");if(wifiBar&&Number.isFinite(data.wifi_rssi))wifiBar.style.width=Math.max(0,Math.min(100,((data.wifi_rssi+100)/60)*100))+"%";
 cydSet("healthCpu",noData);cydSet("healthMemory",noData);
 const gpsStatus=$("gpsStatus");if(gpsStatus)gpsStatus.textContent=gpsReady?"GPS FIX":noData.toUpperCase();
 cydSet("gpsLat",gpsReady?Number(data.latitude).toFixed(5)+"°":noData);
 cydSet("gpsLon",gpsReady?Number(data.longitude).toFixed(5)+"°":noData);
 cydSet("gpsAlt",gpsReady&&Number.isFinite(data.altitude_m)?data.altitude_m.toFixed(0)+" m":noData);
 cydSet("gpsAccuracy",noData);
 cydSet("gpsSatCount",gpsReady?(data.gps_satellites??"—"):noData);
 cydSet("gpsTime",gpsReady?new Date().toLocaleTimeString():"—");
 cydSet("radarHeading",hw.compass_connected===true&&Number.isFinite(data.compass_heading_deg)?data.compass_heading_deg.toFixed(1)+"°":noData);
 cydSet("compassDirection",hw.compass_connected===true&&Number.isFinite(data.compass_heading_deg)?compassNames[Math.round(data.compass_heading_deg/45)%8]:noData);
 cydSetPill("azDriverStatus",hw.motor_drivers_connected===true);cydSetPill("elDriverStatus",hw.motor_drivers_connected===true);
 if(gpsReady&&Number.isFinite(data.latitude)&&Number.isFinite(data.longitude)){
  window.cydObserverLocation={latitude:data.latitude,longitude:data.longitude,height:Number.isFinite(data.altitude_m)?data.altitude_m/1000:0};
  window.dispatchEvent(new Event("cyd-observer-change"));
  const lat=$("observerLat"),lon=$("observerLon");if(lat)lat.value=data.latitude;if(lon)lon.value=data.longitude;
  const note=$("deviceSensorNote");if(note)note.textContent=cydText("Координаты получены от GPS-модуля CYD.","Coordinates received from the CYD GPS module.");
 }else{
  const note=$("deviceSensorNote");if(note)note.textContent=cydText("GPS: Не подключен или нет действительных координат. Компас: Не подключен. Показания не подменяются демонстрационными.","GPS: Not connected or no valid fix. Compass: Not connected. No demo readings are substituted.");
 }
 // Firmware currently reports target setpoints, not measured shaft angles.
 const control=data.control||{};
 if(Number.isFinite(control.target_azimuth_deg)&&Number.isFinite(control.target_elevation_deg)){
  setAxes(control.target_azimuth_deg,control.target_elevation_deg);
  cydSet("overviewAz",noData);cydSet("overviewEl",noData);cydSet("kpiAz",noData);cydSet("kpiEl",noData);
 }
 // Voltage, motor current and temperature are not exposed by the current firmware API.
 ["kpiMotorCurrent","kpiSupplyVoltage","voltageValue","voltageGauge","azCurrentValue","azCurrentGauge","elCurrentValue","elCurrentGauge","temperatureValue","temperatureGauge"].forEach(id=>cydSet(id,noData));
 ["voltageHint","azCurrentHint","elCurrentHint","temperatureHint","kpiMotorCurrentHint","kpiSupplyVoltageHint"].forEach(id=>cydSet(id,noData));
 cydSet("telemetryNote",cydText("Отображаются только данные, которые реально передаёт прошивка. Напряжение, токи, температура, CPU/RAM, углы валов и показания отсутствующих датчиков не передаются: Не подключен.","Only values actually reported by the firmware are shown. Voltage, currents, temperature, CPU/RAM, shaft angles and missing sensors are not reported: Not connected."));
}
async function sendCydCommand(payload){
 if(!window.cydConnected||!cydBaseUrl)return;
 try{
  const response=await fetch(cydBaseUrl+"/api/v1/command",{method:"POST",mode:"cors",credentials:"omit",cache:"no-store",headers:{"Content-Type":"application/json"},body:JSON.stringify(payload)});
  const result=await response.json();if(!response.ok||result.ok!==true)throw Error(result.message||result.error||("HTTP "+response.status));
  const locked=result.motors_enabled===false;
  cydSet("controlStatus",result.message+(locked?" · "+cydText("приводы заблокированы","drives locked"):""));
  addLog(payload.command==="stop"?"STOP":"CTRL",result.message);
 }catch(error){cydSet("controlStatus",cydText("Ошибка команды CYD: ","CYD command error: ")+error.message);}
}
function markCydOffline(errorText){
 window.cydConnected=false;
 if(cydEverConnected||localStorage.getItem("cydEverConnected")==="1"){
  setCydMode("offline");cydSetUnavailable();
  const badge=$("liveDeviceBadge");if(badge){const span=badge.querySelector("span");if(span)span.textContent=cydText("Нет связи с CYD","CYD offline");}
  cydSet("cydDeviceStatus",errorText||cydText("Связь с CYD потеряна. Проверьте Wi-Fi и адрес устройства.","CYD connection lost. Check Wi-Fi and device address."));
  cydSet("connectionResult",errorText||cydText("Нет ответа от контроллера.","No response from controller."));
 }
}
async function connectCydDevice(address,silent=false){
 let base;
 try{base=normalizeCydUrl(address);}catch(error){if(!silent)cydSet("cydDeviceStatus",error.message);return false;}
 if(!silent)cydSet("cydDeviceStatus",cydText("Проверяю связь с CYD…","Checking CYD connection…"));
 try{
  const data=await requestCydStatus(base);
  cydBaseUrl=base;window.cydConnected=true;cydEverConnected=true;
  try{localStorage.setItem("cydDeviceBaseUrl",base);localStorage.setItem("cydEverConnected","1");}catch{}
  showCydAddress(base);setCydMode("connected");updateCydSensorReadings(data);
  cydSet("cydDeviceStatus",cydText("Подключено. Демо-режим отключён; обновление каждые 2 секунды.","Connected. Demo mode disabled; refreshing every 2 seconds."));
  cydSet("connectionResult",cydText("Получен ответ от реального контроллера CYD.","Received a response from the real CYD controller."));
  cydSet("controlStatus",cydText("CYD подключён. Приводы заблокированы прошивкой; реальные углы валов не измеряются.","CYD connected. Drives are locked by firmware; actual shaft angles are not measured."));
  addLog("NET",cydText("Подключение к CYD установлено: ","CYD connection established: ")+base);
  if(cydPollTimer)clearInterval(cydPollTimer);
  cydPollTimer=setInterval(async()=>{if(!cydBaseUrl)return;try{const fresh=await requestCydStatus(cydBaseUrl);updateCydSensorReadings(fresh);setCydMode("connected");}catch(error){markCydOffline(cydText("Связь с CYD потеряна: ","CYD connection lost: ")+error.message);clearInterval(cydPollTimer);cydPollTimer=null;}},2000);
  return true;
 }catch(error){
  if(!silent)cydSet("cydDeviceStatus",cydText("Не удалось подключиться: ","Connection failed: ")+error.message+(location.protocol==="https:"?cydText(". Разрешите локальную сеть в запросе браузера или откройте локальный адрес CYD напрямую.",". Allow local network access in the browser prompt or open the CYD local address directly."):""));
  markCydOffline(error.message);return false;
 }
}
function initCydConnection(){
 let saved="";try{saved=localStorage.getItem("cydDeviceBaseUrl")||"";cydEverConnected=localStorage.getItem("cydEverConnected")==="1";}catch{}
 const input=$("cydDeviceAddress");if(input)input.value=saved||"http://cyd-tracker.local";
 const other=$("cydConnectionAddress");if(other)other.value=saved||"http://cyd-tracker.local";
 if(cydEverConnected){setCydMode("offline");cydSetUnavailable();}
 const currentHost=location.hostname==="cyd-tracker.local"||/^192\.168\.|^10\.|^172\.(1[6-9]|2\d|3[01])\./.test(location.hostname);
 if(saved)connectCydDevice(saved,true);
 else if(currentHost&&location.protocol==="http:")connectCydDevice(location.origin,true);
}
["connectCydDevice","connectCydFromConnection"].forEach(id=>{const button=$(id);if(button)button.addEventListener("click",()=>{const value=$(id==="connectCydDevice"?"cydDeviceAddress":"cydConnectionAddress")?.value;connectCydDevice(value);});});
["cydDeviceAddress","cydConnectionAddress"].forEach(id=>{const input=$(id);if(input)input.addEventListener("change",()=>showCydAddress(input.value));});
initCydConnection();
