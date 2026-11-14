# CYD Satellite Tracker — React Native / Expo

iPhone-first mobile application prototype. Developed from an iPad browser using Expo Snack, with Expo/EAS as the path to native iOS builds.

## Preview on iPad
1. Open https://snack.expo.dev/ in Safari.
2. Create a new Snack.
3. Replace the contents of App.js with the code in this folder.
4. Select iOS in the preview panel. To run on a phone, install Expo Go from the App Store and scan the Snack QR code (availability/QR behavior may vary by device and browser).
5. Save the Snack to an Expo account.

## Prototype screens
- Overview with ISS demo card and orbital illustration
- Satellite catalog with selectable ISS, NOAA 19 and AO-91
- Radar/compass mockup
- Antenna status and safe, non-operational angle display
- Russian/English language toggle

## Not yet connected
Live TLE/SGP4, GPS/compass sensors, Cloudflare authentication, device pairing, telemetry, command queue and notifications. No motor commands are sent.

## Production path
Use a full Expo project and EAS Build for a standalone iOS application. iOS signing and device distribution require the appropriate Apple Developer account and provisioning. Expo Go is a preview tool, not the production app.
