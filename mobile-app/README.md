# CYD Satellite Tracker — React Native / Expo

iPhone-first mobile application prototype. Developed from an iPad browser using Expo Snack, with Expo/EAS as the path to native iOS builds.

## Preview on iPad
1. Open https://snack.expo.dev/ in Safari.
2. Create a new Snack.
3. Replace the contents of App.js with the code in this folder.
4. In the Snack Dependencies panel, add `expo-secure-store` (choose the version compatible with the Snack Expo SDK).
5. Select iOS in the preview panel. To run on a phone, install Expo Go from the App Store and scan the Snack QR code (availability/QR behavior may vary by device and browser).
5. Save the Snack to an Expo account.

## Prototype screens
- Overview with ISS demo card and orbital illustration
- Satellite catalog with selectable ISS, NOAA 19 and AO-91
- Radar/compass mockup
- Antenna status and safe, non-operational angle display
- Russian/English language toggle
- Cloud account registration, sign-in and sign-out using the existing Cloudflare API
- Mobile session token stored with `expo-secure-store`

## Not yet connected
Live TLE/SGP4, GPS/compass sensors, device pairing, telemetry, command queue and notifications. No motor commands are sent.



## Cloud account setup
The app uses `https://cyd-satellite-tracker.pages.dev` and the endpoints `/api/auth/register`, `/api/auth/login`, `/api/auth/me` and `/api/auth/logout`. Mobile login/register requests include `client: "mobile"`; the API returns a bearer session token. The app stores it in iOS SecureStore and sends it in the Authorization header. The website's cookie-based sessions remain supported.

## Production path
Use a full Expo project and EAS Build for a standalone iOS application. iOS signing and device distribution require the appropriate Apple Developer account and provisioning. Expo Go is a preview tool, not the production app.
