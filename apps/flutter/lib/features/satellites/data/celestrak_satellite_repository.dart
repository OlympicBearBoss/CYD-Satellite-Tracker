import 'dart:convert';
import 'package:http/http.dart' as http;

/// Orbit record returned by CelesTrak GP JSON.
class SatelliteOrbit {
  const SatelliteOrbit({
    required this.name,
    required this.noradId,
    required this.epoch,
    required this.line1,
    required this.line2,
    required this.category,
    this.inclination,
    this.eccentricity,
    this.meanMotion,
  });

  final String name;
  final int noradId;
  final DateTime? epoch;
  final String line1;
  final String line2;
  final String category;
  final double? inclination;
  final double? eccentricity;
  final double? meanMotion;

  factory SatelliteOrbit.fromJson(
    Map<String, dynamic> json, {
    required String category,
  }) {
    return SatelliteOrbit(
      name: json['OBJECT_NAME']?.toString() ?? 'Unknown',
      noradId: int.parse(json['NORAD_CAT_ID'].toString()),
      epoch: DateTime.tryParse(json['EPOCH']?.toString() ?? ''),
      line1: json['TLE_LINE1']?.toString() ?? '',
      line2: json['TLE_LINE2']?.toString() ?? '',
      category: category,
      inclination: double.tryParse(json['INCLINATION']?.toString() ?? ''),
      eccentricity: double.tryParse(json['ECCENTRICITY']?.toString() ?? ''),
      meanMotion: double.tryParse(json['MEAN_MOTION']?.toString() ?? ''),
    );
  }
}

/// Shared open-data repository for iOS, iPadOS and Android.
class CelestrakSatelliteRepository {
  CelestrakSatelliteRepository({http.Client? client})
      : _client = client ?? http.Client();

  final http.Client _client;

  static const _groups = <String>['stations', 'weather', 'amateur'];

  Future<List<SatelliteOrbit>> fetchCatalog() async {
    final byNorad = <int, SatelliteOrbit>{};
    for (final group in _groups) {
      final uri = Uri.https(
        'celestrak.org',
        '/NORAD/elements/gp.php',
        {'GROUP': group, 'FORMAT': 'json'},
      );
      final response = await _client.get(uri).timeout(
            const Duration(seconds: 20),
          );
      if (response.statusCode != 200) {
        throw Exception('CelesTrak HTTP ${response.statusCode} ($group)');
      }
      final decoded = jsonDecode(response.body);
      if (decoded is! List) continue;
      for (final row in decoded.whereType<Map<String, dynamic>>()) {
        final satellite = SatelliteOrbit.fromJson(row, category: group);
        if (satellite.line1.isNotEmpty && satellite.line2.isNotEmpty) {
          byNorad[satellite.noradId] = satellite;
        }
      }
    }
    final result = byNorad.values.toList()
      ..sort((a, b) => a.name.compareTo(b.name));
    return result;
  }

  void close() => _client.close();
}
