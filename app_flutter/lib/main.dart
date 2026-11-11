import 'dart:async';
import 'dart:convert';
import 'package:flutter/material.dart';
import 'package:http/http.dart' as http;
import 'package:multicast_dns/multicast_dns.dart';

void main() => runApp(const CydApp());

class CydApp extends StatelessWidget {
  const CydApp({super.key});
  @override
  Widget build(BuildContext context) => MaterialApp(
    title: 'CYD Satellite Tracker',
    debugShowCheckedModeBanner: false,
    theme: ThemeData.dark().copyWith(
      scaffoldBackgroundColor: const Color(0xFF071426),
      colorScheme: ColorScheme.fromSeed(seedColor: const Color(0xFF00BDEB), brightness: Brightness.dark),
    ),
    home: const DevicePage(),
  );
}

class FoundDevice {
  const FoundDevice({required this.name, required this.address, required this.firmware, required this.status});
  final String name, address, firmware;
  final Map<String, dynamic> status;
}

class DevicePage extends StatefulWidget {
  const DevicePage({super.key});
  @override
  State<DevicePage> createState() => _DevicePageState();
}

class _DevicePageState extends State<DevicePage> {
  final address = TextEditingController(text: 'http://cyd-tracker.local');
  Map<String, dynamic>? status;
  List<FoundDevice> found = [];
  String message = 'Нажмите «Найти CYD» для поиска устройств в этой Wi‑Fi сети';
  Timer? timer;
  bool busy = false, scanning = false;

  Uri endpoint(String host, String path) {
    final raw = host.trim().replaceAll(RegExp(r'/+$'), '');
    final base = raw.startsWith('http://') || raw.startsWith('https://') ? raw : 'http://$raw';
    return Uri.parse('$base$path');
  }

  Future<Map<String, dynamic>?> probe(String host) async {
    try {
      final response = await http.get(endpoint(host, '/api/v1/status')).timeout(const Duration(seconds: 3));
      if (response.statusCode != 200) return null;
      final data = jsonDecode(response.body);
      if (data is! Map<String, dynamic> || data['device'] != 'CYD Satellite Tracker') return null;
      return data;
    } catch (_) {
      return null;
    }
  }

  Future<void> discover() async {
    if (scanning) return;
    setState(() { scanning = true; found = []; message = 'Поиск CYD в локальной сети…'; });
    final client = MDnsClient();
    final candidates = <String, String>{};
    try {
      await client.start();
      await for (final PtrResourceRecord ptr in client.lookup<PtrResourceRecord>(
        ResourceRecordQuery.serverPointer('_http._tcp.local'),
      ).timeout(const Duration(seconds: 5), onTimeout: (sink) => sink.close())) {
        await for (final SrvResourceRecord srv in client.lookup<SrvResourceRecord>(
          ResourceRecordQuery.service(ptr.domainName),
        ).timeout(const Duration(seconds: 2), onTimeout: (sink) => sink.close())) {
          if (srv.port != 80) continue;
          await for (final IPAddressResourceRecord ip in client.lookup<IPAddressResourceRecord>(
            ResourceRecordQuery.addressIPv4(srv.target),
          ).timeout(const Duration(seconds: 2), onTimeout: (sink) => sink.close())) {
            candidates['http://${ip.address.address}'] = srv.target;
          }
        }
      }
    } catch (_) {
      // mDNS lookup can end by timeout when no service is present.
    } finally {
      client.stop();
    }
    final matches = <FoundDevice>[];
    for (final host in candidates.keys) {
      final data = await probe(host);
      if (data != null) {
        matches.add(FoundDevice(
          name: (data['device'] ?? 'CYD Satellite Tracker').toString(),
          address: host,
          firmware: (data['firmware'] ?? 'неизвестно').toString(),
          status: data,
        ));
      }
    }
    if (!mounted) return;
    setState(() {
      found = matches;
      scanning = false;
      message = matches.isEmpty
        ? 'CYD не найден. Проверьте, что телефон и контроллер подключены к одной Wi‑Fi сети, затем повторите поиск или введите IP вручную.'
        : 'Найдено устройств: ${matches.length}';
    });
  }

  Future<void> connect(String host) async {
    setState(() { address.text = host; busy = true; });
    final data = await probe(host);
    if (!mounted) return;
    setState(() {
      status = data;
      busy = false;
      message = data == null ? 'Нет связи с $host' : 'Подключено к локальной сети: $host';
    });
    if (data != null) {
      timer?.cancel();
      timer = Timer.periodic(const Duration(seconds: 2), (_) => refresh());
    }
  }

  Future<void> refresh() async {
    if (busy) return;
    final data = await probe(address.text);
    if (!mounted) return;
    setState(() {
      status = data;
      if (data == null) message = 'Нет связи с устройством';
    });
  }

  Future<void> stop() async {
    setState(() => busy = true);
    try {
      final response = await http.post(
        endpoint(address.text, '/api/v1/command'),
        headers: {'Content-Type': 'application/json'},
        body: jsonEncode({'command': 'stop'}),
      ).timeout(const Duration(seconds: 4));
      final data = jsonDecode(response.body) as Map<String, dynamic>;
      if (mounted) setState(() => message = (data['message'] ?? data['error'] ?? 'Ответ получен').toString());
    } catch (e) {
      if (mounted) setState(() => message = 'Команда не отправлена: $e');
    } finally {
      if (mounted) setState(() => busy = false);
    }
  }

  @override
  void initState() {
    super.initState();
    connect(address.text);
  }

  @override
  void dispose() {
    timer?.cancel();
    address.dispose();
    super.dispose();
  }

  Widget metric(String label, String value) => Card(
    child: Padding(
      padding: const EdgeInsets.all(16),
      child: Column(crossAxisAlignment: CrossAxisAlignment.start, children: [
        Text(label, style: const TextStyle(color: Colors.white60)),
        const SizedBox(height: 6),
        Text(value, style: const TextStyle(fontSize: 18, fontWeight: FontWeight.w600)),
      ]),
    ),
  );

  @override
  Widget build(BuildContext context) {
    final data = status ?? {};
    return Scaffold(
      appBar: AppBar(title: const Text('CYD Satellite Tracker'), actions: [
        IconButton(onPressed: scanning ? null : discover, tooltip: 'Найти устройства', icon: const Icon(Icons.radar)),
      ]),
      body: ListView(padding: const EdgeInsets.all(16), children: [
        const Text('УСТРОЙСТВА В ЛОКАЛЬНОЙ СЕТИ', style: TextStyle(color: Color(0xFF65D9FF), fontWeight: FontWeight.bold)),
        const SizedBox(height: 8),
        FilledButton.icon(
          onPressed: scanning ? null : discover,
          icon: scanning ? const SizedBox(width: 18, height: 18, child: CircularProgressIndicator(strokeWidth: 2)) : const Icon(Icons.search),
          label: Text(scanning ? 'ПОИСК…' : 'НАЙТИ CYD'),
        ),
        Card(child: Padding(padding: const EdgeInsets.all(14), child: Text(message))),
        if (found.isNotEmpty) ...[
          const Padding(padding: EdgeInsets.only(top: 8, bottom: 4), child: Text('НАЙДЕННЫЕ УСТРОЙСТВА', style: TextStyle(color: Color(0xFF65D9FF), fontWeight: FontWeight.bold))),
          ...found.map((device) => Card(child: ListTile(
            leading: const CircleAvatar(child: Icon(Icons.memory)),
            title: Text(device.name),
            subtitle: Text('${device.address} · ${device.firmware}'),
            trailing: const Icon(Icons.chevron_right),
            onTap: () => connect(device.address),
          ))),
        ],
        const Divider(height: 28),
        const Text('РУЧНОЕ ПОДКЛЮЧЕНИЕ', style: TextStyle(color: Color(0xFF65D9FF), fontWeight: FontWeight.bold)),
        const SizedBox(height: 8),
        TextField(controller: address, decoration: const InputDecoration(labelText: 'Адрес устройства', hintText: 'http://192.168.1.50', border: OutlineInputBorder())),
        const SizedBox(height: 10),
        FilledButton(onPressed: busy ? null : () => connect(address.text), child: const Text('ПОДКЛЮЧИТЬСЯ')),
        Card(child: Padding(padding: const EdgeInsets.all(16), child: Text(status == null ? 'Устройство не подключено' : 'Подключено · ${data['ip'] ?? address.text}'))),
        GridView.count(crossAxisCount: 2, shrinkWrap: true, physics: const NeverScrollableScrollPhysics(), childAspectRatio: 1.8, children: [
          metric('Wi‑Fi', data['wifi_rssi'] == null ? '—' : '${data['wifi_rssi']} dBm'),
          metric('GPS', data['gps_valid'] == true ? 'Фиксация есть' : 'Поиск'),
          metric('Широта', (data['latitude'] ?? '—').toString()),
          metric('Долгота', (data['longitude'] ?? '—').toString()),
          metric('Спутники GPS', (data['gps_satellites'] ?? '—').toString()),
          metric('Облако', data['cloud_linked'] == true ? 'Подключено' : 'Не привязано'),
        ]),
        const SizedBox(height: 12),
        FilledButton(
          style: FilledButton.styleFrom(backgroundColor: Colors.red.shade800, padding: const EdgeInsets.all(18)),
          onPressed: busy || status == null ? null : stop,
          child: const Text('STOP (КОМАНДА-ПРОВЕРКА)'),
        ),
        const SizedBox(height: 12),
        const Text('Команда STOP пока только подтверждается прошивкой и не является аппаратным аварийным отключением. Управление приводами будет включено после проверки концевиков и защиты.', style: TextStyle(color: Colors.white60)),
      ]),
    );
  }
}
