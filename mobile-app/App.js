import React, { useEffect, useMemo, useState } from "react";
import {
  SafeAreaView, View, Text, ScrollView, Pressable, StyleSheet,
  StatusBar, useWindowDimensions, TextInput, ActivityIndicator
} from "react-native";
import * as SecureStore from "expo-secure-store";

const API = "https://cyd-satellite-tracker.pages.dev";
const TOKEN_KEY = "cyd_mobile_session";

const C = {
  bg: "#07111F", panel: "#101F31", panel2: "#14283D",
  line: "#24435B", cyan: "#35D8F2", blue: "#328BFF",
  text: "#F2F7FC", muted: "#8EA6BB", green: "#48D6A0",
  orange: "#FFB45C"
};

const TABS = [
  ["Обзор", "Overview", "◉"],
  ["Спутники", "Satellites", "✦"],
  ["Радар", "Radar", "◎"],
  ["Антенна", "Antenna", "⌁"],
  ["Настройки", "Settings", "⚙"]
];

function Card({ children, style }) {
  return <View style={[s.card, style]}>{children}</View>;
}
function Eyebrow({ children }) {
  return <Text style={s.eyebrow}>{children}</Text>;
}
function Header({ title, subtitle, lang, onLang }) {
  return <View style={s.header}>
    <View>
      <Text style={s.brand}>CYD  /  SATELLITE TRACKER</Text>
      <Text style={s.title}>{title}</Text>
      <Text style={s.sub}>{subtitle}</Text>
    </View>
    <Pressable onPress={onLang} style={s.lang}><Text style={s.langText}>{lang === "ru" ? "EN" : "RU"}</Text></Pressable>
  </View>;
}
function Metric({ label, value, note }) {
  return <Card style={{ flex: 1 }}>
    <Eyebrow>{label}</Eyebrow>
    <Text style={s.metric}>{value}</Text>
    <Text style={s.muted}>{note}</Text>
  </Card>;
}
function Overview({ lang, onSelect }) {
  const ru = lang === "ru";
  return <ScrollView contentContainerStyle={s.content}>
    <Header title={ru ? "Космический обзор" : "Space overview"} subtitle={ru ? "ВАША НАЗЕМНАЯ СТАНЦИЯ" : "YOUR PERSONAL GROUND STATION"} lang={lang} onLang={onSelect}/>
    <View style={s.row}>
      <Metric label={ru ? "ОТСЛЕЖИВАЕТСЯ" : "TRACKING"} value="01" note={ru ? "спутник" : "satellite"}/>
      <Metric label={ru ? "СЛЕДУЮЩИЙ ПРОЛЁТ" : "NEXT PASS"} value="—" note={ru ? "нет расчёта" : "not calculated"}/>
    </View>
    <Card>
      <Eyebrow>{ru ? "ВЫБРАННЫЙ СПУТНИК" : "SELECTED SATELLITE"}</Eyebrow>
      <View style={s.satHero}>
        <View style={s.satIcon}><Text style={{fontSize:27}}>✦</Text></View>
        <View style={{flex:1}}>
          <Text style={s.cardTitle}>ISS (ZARYA)</Text>
          <Text style={s.muted}>{ru ? "Международная космическая станция" : "International Space Station"}</Text>
          <Text style={s.frequency}>VHF  •  145.800 MHz</Text>
        </View>
        <Text style={s.liveDot}>●</Text>
      </View>
    </Card>
    <Card style={{padding:14}}>
      <View style={s.rowBetween}>
        <Eyebrow>{ru ? "ОРБИТАЛЬНАЯ ОБСТАНОВКА" : "ORBITAL VIEW"}</Eyebrow>
        <Text style={s.tag}>DEMO</Text>
      </View>
      <View style={s.earthScene}>
        <View style={s.orbit}/>
        <View style={s.earth}><View style={s.land}/></View>
        <Text style={s.satellite}>✦</Text>
        <Text style={s.sceneLabel}>ISS • DEMO ORBIT</Text>
      </View>
    </Card>
    <View style={s.status}><Text style={s.orange}>●</Text><Text style={s.muted}>{ru ? "Устройство CYD не подключено — демонстрационный режим" : "CYD device disconnected — demo mode"}</Text></View>
  </ScrollView>;
}
function Satellites({ lang, selected, setSelected }) {
  const ru = lang === "ru";
  const items = [
    ["ISS (ZARYA)", ru ? "Орбитальная станция" : "Space station", "145.800 MHz"],
    ["NOAA 19", ru ? "Метеорологический" : "Weather satellite", "137.100 MHz"],
    ["AO-91", ru ? "Любительский" : "Amateur radio", "145.960 MHz"]
  ];
  return <ScrollView contentContainerStyle={s.content}>
    <Header title={ru ? "Спутники" : "Satellites"} subtitle={ru ? "КАТАЛОГ ОБЪЕКТОВ" : "OBJECT CATALOG"} lang={lang} onLang={()=>{}}/>
    {items.map(([name, type, freq])=><Pressable key={name} onPress={()=>setSelected(name)} style={s.card}>
      <View style={s.rowBetween}>
        <View style={{flexDirection:"row",alignItems:"center",gap:12,flex:1}}>
          <View style={s.smallIcon}><Text style={{color:C.cyan,fontSize:22}}>✦</Text></View>
          <View style={{flex:1}}>
            <Text style={s.cardTitle}>{name}</Text>
            <Text style={s.muted}>{type}</Text>
            <Text style={s.frequency}>{freq}</Text>
          </View>
        </View>
        <Text style={{color:selected===name?C.green:C.cyan,fontWeight:"800",fontSize:11}}>{selected===name?(ru?"СЛЕЖЕНИЕ":"FOLLOWING"):"＋"}</Text>
      </View>
    </Pressable>)}
    <Text style={s.muted}>{ru ? "Пока показаны демонстрационные объекты. Реальные орбиты и TLE подключим следующим этапом." : "Sample objects for now. Live TLE and orbit calculations will be added next."}</Text>
  </ScrollView>;
}
function Radar({ lang }) {
  const ru=lang==="ru";
  return <ScrollView contentContainerStyle={s.content}>
    <Header title={ru?"Радар и компас":"Radar & compass"} subtitle={ru?"НАПРАВЛЕНИЕ НА СПУТНИК":"SATELLITE BEARING"} lang={lang} onLang={()=>{}}/>
    <View style={s.radar}>
      {[0.88,0.65,0.42,0.18].map((v,i)=><View key={i} style={[s.ring,{width:`${v*100}%`,height:`${v*100}%`}]}/>)}
      <View style={s.crossV}/><View style={s.crossH}/>
      <Text style={[s.cardTitle,{position:"absolute",top:12}]}>N</Text>
      <Text style={[s.cardTitle,{position:"absolute",bottom:12}]}>S</Text>
      <Text style={[s.cardTitle,{position:"absolute",left:12}]}>W</Text>
      <Text style={[s.cardTitle,{position:"absolute",right:12}]}>E</Text>
      <View style={s.radarTarget}/><View style={s.radarCenter}/>
    </View>
    <Card><Text style={s.muted}>{ru?"Ожидание разрешения GPS и данных компаса iPhone. Сейчас отображается макет.":"Waiting for iPhone GPS permission and compass data. This is a preview."}</Text></Card>
  </ScrollView>;
}
function Antenna({ lang }) {
  const ru=lang==="ru";
  return <ScrollView contentContainerStyle={s.content}>
    <Header title={ru?"Управление антенной":"Antenna control"} subtitle="AZIMUTH  /  ELEVATION" lang={lang} onLang={()=>{}}/>
    <Card><View style={s.rowBetween}><Text style={s.cardTitle}>{ru?"Устройство CYD":"CYD device"}</Text><Text style={s.orange}>● OFFLINE</Text></View><Text style={s.muted}>{ru?"Контроллер не подключён. Управление заблокировано.":"Controller disconnected. Controls are locked."}</Text></Card>
    <Card><Eyebrow>{ru?"АЗИМУТ":"AZIMUTH"}</Eyebrow><Text style={s.angle}>184°</Text><Text style={s.muted}>{ru?"Диапазон 0–360°":"Range 0–360°"}</Text></Card>
    <Card><Eyebrow>{ru?"УГОЛ МЕСТА":"ELEVATION"}</Eyebrow><Text style={s.angle}>42°</Text><Text style={s.muted}>{ru?"Диапазон 0–90°":"Range 0–90°"}</Text></Card>
    <Text style={s.muted}>{ru?"Это только макет. Команды двигателям не отправляются. Реальное управление включим после проверки прошивки и концевиков.":"Preview only. No motor commands are sent. Real control will be enabled after firmware and limit-switch safety checks."}</Text>
  </ScrollView>;
}
function Settings({ lang, setLang, user, email, setEmail, password, setPassword, name, setName, busy, error, onLogin, onRegister, onLogout }) {
  const ru=lang==="ru";
  return <ScrollView contentContainerStyle={s.content} keyboardShouldPersistTaps="handled">
    <Header title={ru?"Настройки":"Settings"} subtitle={ru?"ПРИЛОЖЕНИЕ И УСТРОЙСТВО":"APP & DEVICE"} lang={lang} onLang={()=>setLang(ru?"en":"ru")}/>
    <Card><Text style={s.cardTitle}>{ru?"Язык интерфейса":"Interface language"}</Text><View style={s.row}>
      <Pressable onPress={()=>setLang("ru")} style={[s.choice,lang==="ru"&&s.choiceActive]}><Text style={s.choiceText}>Русский</Text></Pressable>
      <Pressable onPress={()=>setLang("en")} style={[s.choice,lang==="en"&&s.choiceActive]}><Text style={s.choiceText}>English</Text></Pressable>
    </View></Card>
    <Card>
      <View style={s.rowBetween}><Text style={s.cardTitle}>{ru?"Аккаунт CYD":"CYD account"}</Text><Text style={[s.tag,{color:user?C.green:C.orange}]}>{user?(ru?"ВХОД ВЫПОЛНЕН":"SIGNED IN"):(ru?"НЕ ВОШЛИ":"SIGNED OUT")}</Text></View>
      {user ? <>
        <Text style={s.cardTitle}>{user.display_name||user.email}</Text>
        <Text style={s.muted}>{user.email}</Text>
        <Pressable disabled={busy} onPress={onLogout} style={s.primaryButton}>{busy?<ActivityIndicator color={C.bg}/>:<Text style={s.primaryText}>{ru?"Выйти из аккаунта":"Sign out"}</Text>}</Pressable>
      </> : <>
        <Text style={s.muted}>{ru?"Войдите в существующий аккаунт или создайте новый. Пароль должен содержать не менее 12 символов.":"Sign in or create an account. Password must be at least 12 characters."}</Text>
        <TextInput value={name} onChangeText={setName} placeholder={ru?"Имя (для регистрации)":"Name (for sign up)"} placeholderTextColor={C.muted} style={s.field} autoCapitalize="words"/>
        <TextInput value={email} onChangeText={setEmail} placeholder="Email" placeholderTextColor={C.muted} style={s.field} keyboardType="email-address" autoCapitalize="none" autoCorrect={false}/>
        <TextInput value={password} onChangeText={setPassword} placeholder={ru?"Пароль":"Password"} placeholderTextColor={C.muted} style={s.field} secureTextEntry autoCapitalize="none"/>
        {error?<Text style={{color:"#FF8D8D",fontSize:12}}>{error}</Text>:null}
        <View style={s.row}>
          <Pressable disabled={busy} onPress={onLogin} style={[s.primaryButton,{flex:1}]}>{busy?<ActivityIndicator color={C.bg}/>:<Text style={s.primaryText}>{ru?"Войти":"Sign in"}</Text>}</Pressable>
          <Pressable disabled={busy} onPress={onRegister} style={[s.choice,{flex:1,borderColor:C.cyan}]}><Text style={s.choiceText}>{ru?"Регистрация":"Sign up"}</Text></Pressable>
        </View>
      </>}
    </Card>
    <Card><Text style={s.cardTitle}>CYD Satellite Tracker</Text><Text style={s.muted}>React Native • Expo • Cloud API</Text></Card>
  </ScrollView>;
}

export default function App() {
  const [tab,setTab]=useState(0);
  const [lang,setLang]=useState("ru");
  const [selected,setSelected]=useState("ISS (ZARYA)");
  const [user,setUser]=useState(null);
  const [token,setToken]=useState(null);
  const [email,setEmail]=useState("");
  const [password,setPassword]=useState("");
  const [name,setName]=useState("");
  const [busy,setBusy]=useState(false);
  const [authError,setAuthError]=useState("");
  const {width}=useWindowDimensions();

  useEffect(()=>{(async()=>{try{const saved=await SecureStore.getItemAsync(TOKEN_KEY);if(!saved)return;const response=await fetch(API+"/api/auth/me",{headers:{Authorization:"Bearer "+saved}});if(response.ok){const data=await response.json();setToken(saved);setUser(data.user);}else{await SecureStore.deleteItemAsync(TOKEN_KEY);}}catch(e){setAuthError("Не удалось проверить сессию. Проверьте интернет.");}})();},[]);
  const authenticate=async(mode)=>{setBusy(true);setAuthError("");try{const response=await fetch(API+(mode==="register"?"/api/auth/register":"/api/auth/login"),{method:"POST",headers:{"Content-Type":"application/json"},body:JSON.stringify({email,password,name,client:"mobile"})});const data=await response.json();if(!response.ok)throw new Error(data.error||"Ошибка авторизации");if(!data.session_token)throw new Error("Сервер не вернул мобильный токен");await SecureStore.setItemAsync(TOKEN_KEY,data.session_token);setToken(data.session_token);setUser(data.user);setPassword("");}catch(e){setAuthError(e.message||"Не удалось подключиться к серверу");}finally{setBusy(false);}};
  const logout=async()=>{setBusy(true);try{if(token)await fetch(API+"/api/auth/logout",{method:"POST",headers:{Authorization:"Bearer "+token}});await SecureStore.deleteItemAsync(TOKEN_KEY);setToken(null);setUser(null);}catch(e){setAuthError("Не удалось завершить сессию");}finally{setBusy(false);}};
  const compact=width<390;
  const screen=useMemo(()=>[
    <Overview key="home" lang={lang} onSelect={()=>setLang(lang==="ru"?"en":"ru")}/>,
    <Satellites key="sat" lang={lang} selected={selected} setSelected={setSelected}/>,
    <Radar key="radar" lang={lang}/>,
    <Antenna key="antenna" lang={lang}/>,
    <Settings key="settings" lang={lang} setLang={setLang} user={user} email={email} setEmail={setEmail} password={password} setPassword={setPassword} name={name} setName={setName} busy={busy} error={authError} onLogin={()=>authenticate("login")} onRegister={()=>authenticate("register")} onLogout={logout}/>
  ][tab],[tab,lang,selected,user,email,password,name,busy,authError]);
  return <SafeAreaView style={s.safe}>
    <StatusBar barStyle="light-content" backgroundColor={C.bg}/>
    <View style={{flex:1}}>{screen}</View>
    <View style={[s.tabbar,compact&&{paddingHorizontal:4}]}>
      {TABS.map(([ru,en,icon],i)=><Pressable key={en} onPress={()=>setTab(i)} style={s.tab}>
        <Text style={[s.tabIcon,tab===i&&{color:C.cyan}]}>{icon}</Text>
        <Text numberOfLines={1} style={[s.tabText,tab===i&&{color:C.cyan}]}>{lang==="ru"?ru:en}</Text>
      </Pressable>)}
    </View>
  </SafeAreaView>;
}

const s=StyleSheet.create({
 safe:{flex:1,backgroundColor:C.bg},content:{padding:18,paddingBottom:28,gap:14},
 header:{flexDirection:"row",justifyContent:"space-between",alignItems:"center",paddingTop:8,paddingBottom:6},
 brand:{fontSize:10,fontWeight:"900",letterSpacing:2.2,color:C.cyan},
 title:{fontSize:27,fontWeight:"800",color:C.text,marginTop:7},
 sub:{fontSize:9,fontWeight:"800",letterSpacing:1.7,color:C.muted,marginTop:4},
 lang:{backgroundColor:C.panel2,borderWidth:1,borderColor:C.line,borderRadius:20,paddingHorizontal:14,paddingVertical:9},
 langText:{color:C.cyan,fontWeight:"900",fontSize:12},
 card:{backgroundColor:C.panel,borderColor:C.line,borderWidth:1,borderRadius:18,padding:16,gap:10},
 eyebrow:{fontSize:9,fontWeight:"900",letterSpacing:1.3,color:C.muted},
 row:{flexDirection:"row",gap:10},rowBetween:{flexDirection:"row",alignItems:"center",justifyContent:"space-between",gap:8},
 metric:{fontSize:30,fontWeight:"900",color:C.text,marginTop:5},
 muted:{fontSize:12,color:C.muted,lineHeight:18},
 satHero:{flexDirection:"row",alignItems:"center",gap:12,marginTop:8},
 satIcon:{width:54,height:54,borderRadius:16,backgroundColor:"#12354A",alignItems:"center",justifyContent:"center"},
 cardTitle:{fontSize:15,fontWeight:"800",color:C.text},
 frequency:{fontSize:11,color:C.cyan,marginTop:4,fontWeight:"700"},
 liveDot:{color:C.green,fontSize:13},
 tag:{color:C.orange,fontSize:9,fontWeight:"900",backgroundColor:"#3B2A1B",paddingHorizontal:8,paddingVertical:5,borderRadius:8},
 earthScene:{height:210,marginTop:10,borderRadius:14,backgroundColor:"#071A2C",alignItems:"center",justifyContent:"center",overflow:"hidden"},
 earth:{width:142,height:142,borderRadius:80,backgroundColor:"#11618A",borderWidth:1,borderColor:"#43D8F4",alignItems:"center",justifyContent:"center",shadowColor:C.cyan,shadowOpacity:.35,shadowRadius:24},
 land:{width:75,height:40,borderRadius:30,backgroundColor:"#318B72",transform:[{rotate:"-25deg"}]},
 orbit:{position:"absolute",width:"90%",height:76,borderWidth:1,borderColor:C.cyan, borderStyle:"dashed",borderRadius:100,transform:[{rotate:"-18deg"}]},
 satellite:{position:"absolute",top:36,right:40,color:C.text,fontSize:23},
 sceneLabel:{position:"absolute",bottom:12,left:12,color:C.cyan,fontSize:9,fontWeight:"900",letterSpacing:1},
 status:{flexDirection:"row",alignItems:"center",gap:8,padding:12},
 orange:{color:C.orange,fontSize:12,fontWeight:"800"},
 smallIcon:{width:44,height:44,borderRadius:13,backgroundColor:"#12354A",alignItems:"center",justifyContent:"center"},
 radar:{width:"100%",aspectRatio:1,borderRadius:300,backgroundColor:"#0B2639",borderColor:C.line,borderWidth:1,alignItems:"center",justifyContent:"center",overflow:"hidden"},
 ring:{position:"absolute",borderWidth:1,borderColor:"#28617A",borderRadius:300},
 crossV:{position:"absolute",width:1,height:"100%",backgroundColor:"#28617A"},
 crossH:{position:"absolute",height:1,width:"100%",backgroundColor:"#28617A"},
 radarTarget:{position:"absolute",width:9,height:9,borderRadius:8,backgroundColor:C.cyan,top:"27%",right:"28%",shadowColor:C.cyan,shadowOpacity:1,shadowRadius:9},
 radarCenter:{width:9,height:9,borderRadius:8,backgroundColor:C.green},
 angle:{fontSize:34,fontWeight:"900",color:C.cyan},
 choice:{flex:1,padding:12,borderRadius:12,backgroundColor:C.panel2,alignItems:"center",borderWidth:1,borderColor:C.line},
 choiceActive:{borderColor:C.cyan,backgroundColor:"#12384A"},
 choiceText:{color:C.text,fontWeight:"700"},
 field:{backgroundColor:C.panel2,borderWidth:1,borderColor:C.line,borderRadius:12,paddingHorizontal:13,paddingVertical:12,color:C.text,fontSize:14},
 primaryButton:{backgroundColor:C.cyan,borderRadius:12,padding:13,alignItems:"center",justifyContent:"center",minHeight:44},
 primaryText:{color:C.bg,fontWeight:"900",fontSize:13},
 tabbar:{flexDirection:"row",paddingHorizontal:8,paddingTop:9,paddingBottom:5,borderTopWidth:1,borderTopColor:C.line,backgroundColor:"#091625"},
 tab:{flex:1,alignItems:"center",justifyContent:"center",gap:4,minWidth:0},
 tabIcon:{fontSize:19,color:C.muted,height:23},
 tabText:{fontSize:9,color:C.muted,fontWeight:"700"}
});
