# Flutter satellite data module

This folder contains the shared CelesTrak catalog repository for the planned Flutter clients.

## Add dependency

Add `http: ^1.2.0` (or a current compatible release) to the Flutter application's `pubspec.yaml`.

## Usage

```dart
final repository = CelestrakSatelliteRepository();
final satellites = await repository.fetchCatalog();
```

The repository returns current GP/TLE elements, including NORAD ID and epoch. It deliberately does not invent AZ/EL values. A verified Dart SGP4 implementation and a station observer model must be connected next to calculate topocentric azimuth/elevation from the two TLE lines and station latitude/longitude/altitude.

The Flutter app itself is not yet a complete runnable application; this is a reusable data-layer source file for the planned iOS/iPadOS/Android project.
