import 'satellite_pass.dart';

/// Implement this adapter with an SGP4 package for Flutter.
abstract interface class SatellitePositionPropagator {
  Future<TopocentricPosition> positionAt({
    required SatellitePassTarget target,
    required GroundStation station,
    required DateTime utc,
  });
}

class GroundStation {
  const GroundStation({
    required this.latitudeDeg,
    required this.longitudeDeg,
    this.altitudeMeters = 0,
  });

  final double latitudeDeg;
  final double longitudeDeg;
  final double altitudeMeters;
}

class SatellitePassTarget {
  const SatellitePassTarget({required this.noradId, required this.name});
  final int noradId;
  final String name;
}

class TopocentricPosition {
  const TopocentricPosition({
    required this.azimuthDeg,
    required this.elevationDeg,
  });
  final double azimuthDeg;
  final double elevationDeg;
}

class PredictedSatellitePass {
  const PredictedSatellitePass({
    required this.aosUtc,
    required this.aosAzimuthDeg,
    required this.maximumElevationDeg,
    required this.maximumUtc,
    required this.losUtc,
    required this.losAzimuthDeg,
    required this.alreadyVisible,
    required this.continuesBeyondWindow,
  });

  final DateTime? aosUtc;
  final double aosAzimuthDeg;
  final double maximumElevationDeg;
  final DateTime maximumUtc;
  final DateTime? losUtc;
  final double? losAzimuthDeg;
  final bool alreadyVisible;
  final bool continuesBeyondWindow;

  Duration? get duration {
    if (aosUtc == null || losUtc == null) return null;
    return losUtc!.difference(aosUtc!);
  }
}

/// Finds threshold crossings from successive SGP4 topocentric samples.
/// AOS/LOS are linearly interpolated between samples; use 10–30 s steps.
class SatellitePassPredictor {
  const SatellitePassPredictor(this.propagator);
  final SatellitePositionPropagator propagator;

  Future<List<PredictedSatellitePass>> predict({
    required SatellitePassTarget target,
    required GroundStation station,
    required DateTime startUtc,
    Duration horizon = const Duration(hours: 24),
    Duration step = const Duration(seconds: 30),
    double minimumElevationDeg = 5,
  }) async {
    if (station.latitudeDeg < -90 || station.latitudeDeg > 90 ||
        station.longitudeDeg < -180 || station.longitudeDeg > 180) {
      throw ArgumentError('Invalid ground-station coordinates');
    }
    if (step <= Duration.zero || horizon <= Duration.zero) {
      throw ArgumentError('Horizon and sample step must be positive');
    }

    DateTime crossing(DateTime a, double ea, DateTime b, double eb) {
      if (ea == eb) return b;
      final f = ((minimumElevationDeg - ea) / (eb - ea)).clamp(0.0, 1.0);
      return a.add(Duration(microseconds:
          (b.difference(a).inMicroseconds * f).round()));
    }

    final end = startUtc.toUtc().add(horizon);
    var previousTime = startUtc.toUtc();
    var previous = await propagator.positionAt(
      target: target, station: station, utc: previousTime);
    _ActivePass? active;
    if (previous.elevationDeg >= minimumElevationDeg) {
      active = _ActivePass(
        aos: null,
        aosAz: previous.azimuthDeg,
        maxElevation: previous.elevationDeg,
        maxTime: previousTime,
        alreadyVisible: true,
      );
    }

    final results = <PredictedSatellitePass>[];
    while (previousTime.isBefore(end)) {
      final nextTime = previousTime.add(step).isAfter(end)
          ? end : previousTime.add(step);
      final current = await propagator.positionAt(
        target: target, station: station, utc: nextTime);
      if (active == null &&
          previous.elevationDeg < minimumElevationDeg &&
          current.elevationDeg >= minimumElevationDeg) {
        active = _ActivePass(
          aos: crossing(previousTime, previous.elevationDeg,
              nextTime, current.elevationDeg),
          aosAz: current.azimuthDeg,
          maxElevation: current.elevationDeg,
          maxTime: nextTime,
        );
      }
      if (active != null && current.elevationDeg > active.maxElevation) {
        active.maxElevation = current.elevationDeg;
        active.maxTime = nextTime;
      }
      if (active != null &&
          previous.elevationDeg >= minimumElevationDeg &&
          current.elevationDeg < minimumElevationDeg) {
        final los = crossing(previousTime, previous.elevationDeg,
            nextTime, current.elevationDeg);
        results.add(active.finish(
          los: los, losAz: current.azimuthDeg, continues: false));
        active = null;
      }
      previousTime = nextTime;
      previous = current;
    }
    if (active != null) {
      results.add(active.finish(los: null, losAz: null, continues: true));
    }
    return results;
  }
}

class _ActivePass {
  _ActivePass({
    required this.aos,
    required this.aosAz,
    required this.maxElevation,
    required this.maxTime,
    this.alreadyVisible = false,
  });
  final DateTime? aos;
  final double aosAz;
  double maxElevation;
  DateTime maxTime;
  final bool alreadyVisible;

  PredictedSatellitePass finish({
    required DateTime? los,
    required double? losAz,
    required bool continues,
  }) => PredictedSatellitePass(
    aosUtc: aos,
    aosAzimuthDeg: aosAz,
    maximumElevationDeg: maxElevation,
    maximumUtc: maxTime,
    losUtc: los,
    losAzimuthDeg: losAz,
    alreadyVisible: alreadyVisible,
    continuesBeyondWindow: continues,
  );
}
