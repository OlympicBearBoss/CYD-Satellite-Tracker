import 'dart:async';
import 'dart:convert';
import 'package:flutter/material.dart';
import 'package:http/http.dart' as http;
void main() => runApp(const CydApp());
class CydApp extends StatelessWidget {
 const CydApp({super.key});
 @override Widget build(BuildContext context) => MaterialApp(title:'CYD Satellite Tracker',debugShowCheckedModeBanner:false,
 theme:ThemeData.dark().copyWith(scaffoldBackgroundColor:const Color(0xFF071426),colorScheme:ColorScheme.fromSeed(seedColor:const Color(0xFF00BDEB),brightness:Brightness.dark)),home:const DevicePage());
}
class DevicePage extends StatefulWidget { const DevicePage({super.key}); @override State<DevicePage> createState()=>_DevicePageState(); }
class _DevicePageState extends State<DevicePage> {
 final address=TextEditingController(text:'http://cyd-tracker.local');
 Map<String,dynamic>? status; String message='Введите адрес CYD или cyd-tracker.local'; Timer? timer; bool busy=false;
 Uri endpoint(String path) { final raw=address.text.trim().replaceAll(RegExp(r'/+$'),''); final base=raw.startsWith('http')?raw:'http://'+raw; return Uri.parse(base+path); }
 Future<void> refresh() async {
  if(busy)return;
  try { final r=await http.get(endpoint('/api/v1/status')).timeout(const Duration(seconds:4)); if(r.statusCode!=200)throw Exception('HTTP '+r.statusCode.toString()); final d=jsonDecode(r.body) as Map<String,dynamic>; if(!mounted)return; setState((){status=d;message='Подключено к локальной сети';}); }
  catch(e){if(!mounted)return;setState((){status=null;message='Нет связи: '+e.toString();});}
 }
 Future<void> stop() async {
  setState(()=>busy=true);
  try { final r=await http.post(endpoint('/api/v1/command'),headers:{'Content-Type':'application/json'},body:jsonEncode({'command':'stop'})).timeout(const Duration(seconds:4)); final d=jsonDecode(r.body) as Map<String,dynamic>; if(!mounted)return; setState(()=>message=(d['message']??d['error']??'Ответ получен').toString()); }
  catch(e){if(mounted)setState(()=>message='Команда не отправлена: '+e.toString());}
  finally{if(mounted)setState(()=>busy=false);}
 }
 @override void initState(){super.initState();refresh();timer=Timer.periodic(const Duration(seconds:2),(_)=>refresh());}
 @override void dispose(){timer?.cancel();address.dispose();super.dispose();}
 Widget metric(String label,String value)=>Card(child:Padding(padding:const EdgeInsets.all(16),child:Column(crossAxisAlignment:CrossAxisAlignment.start,children:[Text(label,style:const TextStyle(color:Colors.white60)),const SizedBox(height:6),Text(value,style:const TextStyle(fontSize:18,fontWeight:FontWeight.w600))])));
 @override Widget build(BuildContext context){
  final d=status??{};
  return Scaffold(appBar:AppBar(title:const Text('CYD Satellite Tracker')),body:ListView(padding:const EdgeInsets.all(16),children:[
   const Text('ЛОКАЛЬНОЕ WI-FI ПОДКЛЮЧЕНИЕ',style:TextStyle(color:Color(0xFF65D9FF),fontWeight:FontWeight.bold)),const SizedBox(height:10),
   TextField(controller:address,decoration:const InputDecoration(labelText:'Адрес устройства',hintText:'http://192.168.1.50',border:OutlineInputBorder())),
   const SizedBox(height:10),FilledButton(onPressed:refresh,child:const Text('ПОДКЛЮЧИТЬСЯ')),
   Card(child:Padding(padding:const EdgeInsets.all(16),child:Text(message))),
   GridView.count(crossAxisCount:2,shrinkWrap:true,physics:const NeverScrollableScrollPhysics(),childAspectRatio:1.8,children:[
    metric('Wi-Fi',d['wifi_rssi']==null?'—':d['wifi_rssi'].toString()+' dBm'),metric('GPS',d['gps_valid']==true?'Фиксация есть':'Поиск'),
    metric('Широта',(d['latitude']??'—').toString()),metric('Долгота',(d['longitude']??'—').toString()),
    metric('Спутники GPS',(d['gps_satellites']??'—').toString()),metric('Облако',d['cloud_linked']==true?'Подключено':'Не привязано')]),
   const SizedBox(height:12),FilledButton(style:FilledButton.styleFrom(backgroundColor:Colors.red.shade800,padding:const EdgeInsets.all(18)),onPressed:busy?null:stop,child:const Text('АВАРИЙНЫЙ СТОП')),
   const SizedBox(height:12),const Text('В текущей версии команды движения отключены в прошивке. Клиент получает телеметрию; управление приводами будет включаться после проверки концевиков и защиты.',style:TextStyle(color:Colors.white60))
  ]));
 }
}
