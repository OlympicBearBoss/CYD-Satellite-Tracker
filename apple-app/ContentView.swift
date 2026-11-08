import SwiftUI

struct ContentView: View {
    @AppStorage("cyd.language") private var language = "ru"
    @State private var selectedTab = 0
    @State private var selectedSatellite = "ISS (ZARYA)"
    @State private var azimuth = 184.0
    @State private var elevation = 42.0
    @State private var showDemoInfo = false

    private var isEnglish: Bool { language == "en" }

    var body: some View {
        ZStack {
            SpaceBackground()
            TabView(selection: $selectedTab) {
                dashboard.tag(0).tabItem { Label(isEnglish ? "Overview" : "Обзор", systemImage: "globe.americas") }
                satellites.tag(1).tabItem { Label(isEnglish ? "Satellites" : "Спутники", systemImage: "satellite") }
                radar.tag(2).tabItem { Label(isEnglish ? "Radar" : "Радар", systemImage: "scope") }
                control.tag(3).tabItem { Label(isEnglish ? "Rotator" : "Ротатор", systemImage: "move.3d") }
                settings.tag(4).tabItem { Label(isEnglish ? "Settings" : "Настройки", systemImage: "slider.horizontal.3") }
            }
            .tint(Color.cyan)
        }
        .preferredColorScheme(.dark)
        .alert(isEnglish ? "Demo mode" : "Демонстрационный режим", isPresented: $showDemoInfo) {
            Button("OK", role: .cancel) { }
        } message: {
            Text(isEnglish ? "The interface is using sample data. Connect a CYD device to enable real telemetry and motor control." : "Интерфейс использует демонстрационные данные. Подключите устройство CYD, чтобы включить реальную телеметрию и управление моторами.")
        }
    }

    private var dashboard: some View {
        NavigationStack {
            ScrollView {
                VStack(alignment: .leading, spacing: 18) {
                    header
                    HStack(spacing: 12) {
                        MetricCard(title: isEnglish ? "TRACKED" : "ОТСЛЕЖИВАЕТСЯ", value: "01", detail: isEnglish ? "satellite" : "спутник", icon: "satellite")
                        MetricCard(title: isEnglish ? "NEXT PASS" : "СЛЕДУЮЩИЙ ПРОЛЁТ", value: "—", detail: isEnglish ? "waiting for orbit data" : "ожидание орбит", icon: "timer")
                    }
                    VStack(alignment: .leading, spacing: 12) {
                        SectionHeading(title: isEnglish ? "Selected satellite" : "Выбранный спутник", subtitle: "LIVE TRACKING • DEMO")
                        HStack(spacing: 14) {
                            ZStack {
                                Circle().fill(Color.cyan.opacity(0.12)).frame(width: 68, height: 68)
                                Image(systemName: "satellite.fill").font(.system(size: 29)).foregroundStyle(.cyan)
                            }
                            VStack(alignment: .leading, spacing: 5) {
                                Text("ISS (ZARYA)").font(.headline)
                                Text(isEnglish ? "International Space Station" : "Международная космическая станция").font(.caption).foregroundStyle(.secondary)
                                Text(isEnglish ? "VHF • 145.800 MHz" : "VHF • 145,800 МГц").font(.caption2.monospaced()).foregroundStyle(.cyan)
                            }
                            Spacer()
                            Image(systemName: "chevron.right").foregroundStyle(.secondary)
                        }
                        .padding(14).background(.white.opacity(0.055), in: RoundedRectangle(cornerRadius: 18))
                    }
                    VStack(alignment: .leading, spacing: 10) {
                        SectionHeading(title: isEnglish ? "Orbital situation" : "Орбитальная обстановка", subtitle: isEnglish ? "EARTH VIEW" : "ВИД НА ЗЕМЛЮ")
                        OrbitIllustration()
                            .frame(height: 220)
                            .clipShape(RoundedRectangle(cornerRadius: 20))
                    }
                    HStack {
                        Label(isEnglish ? "Device not connected" : "Устройство не подключено", systemImage: "wifi.slash")
                        Spacer()
                        Text(isEnglish ? "DEMO" : "ДЕМО").font(.caption2.bold()).foregroundStyle(.orange)
                    }
                    .font(.caption).foregroundStyle(.secondary)
                    .padding(14).background(.white.opacity(0.045), in: RoundedRectangle(cornerRadius: 14))
                    Button { showDemoInfo = true } label: {
                        Label(isEnglish ? "About demo mode" : "О демонстрационном режиме", systemImage: "info.circle")
                            .font(.subheadline)
                    }.buttonStyle(.plain).foregroundStyle(.cyan)
                }
                .padding(18)
            }
            .background(Color.clear)
            .toolbar(.hidden, for: .navigationBar)
        }
    }

    private var header: some View {
        HStack(alignment: .center) {
            VStack(alignment: .leading, spacing: 4) {
                Text("CYD").font(.system(size: 13, weight: .black, design: .rounded)).tracking(3).foregroundStyle(.cyan)
                Text("Satellite Tracker").font(.system(size: 27, weight: .bold, design: .rounded))
                Text(isEnglish ? "Your personal ground station" : "Ваша персональная наземная станция").font(.caption).foregroundStyle(.secondary)
            }
            Spacer()
            Button { language = isEnglish ? "ru" : "en" } label: {
                Text(isEnglish ? "RU" : "EN").font(.caption.bold()).padding(.horizontal, 13).padding(.vertical, 9)
                    .background(.white.opacity(0.08), in: Capsule())
            }.buttonStyle(.plain)
        }.padding(.top, 12)
    }

    private var satellites: some View {
        NavigationStack {
            ScrollView {
                VStack(alignment: .leading, spacing: 14) {
                    pageTitle(isEnglish ? "Satellite catalog" : "Каталог спутников", subtitle: isEnglish ? "OPEN ORBITAL DATA" : "ОТКРЫТЫЕ ОРБИТАЛЬНЫЕ ДАННЫЕ")
                    satelliteRow(name: "ISS (ZARYA)", category: isEnglish ? "Space station" : "Орбитальная станция", frequency: "145.800 MHz", selected: true)
                    satelliteRow(name: "NOAA 19", category: isEnglish ? "Weather" : "Метеорологический", frequency: "137.100 MHz", selected: false)
                    satelliteRow(name: "AO-91", category: isEnglish ? "Amateur radio" : "Любительский", frequency: "145.960 MHz", selected: false)
                    Text(isEnglish ? "Sample list for the first UI build. Live TLE updates will be connected in a later stage." : "Пример списка для первой версии интерфейса. Обновление актуальных TLE подключим на следующем этапе.")
                        .font(.caption).foregroundStyle(.secondary).padding(.top, 6)
                }.padding(18)
            }.toolbar(.hidden, for: .navigationBar)
        }
    }

    private func satelliteRow(name: String, category: String, frequency: String, selected: Bool) -> some View {
        HStack(spacing: 12) {
            Image(systemName: "satellite.fill").font(.title2).foregroundStyle(selected ? .cyan : .white.opacity(0.65))
                .frame(width: 46, height: 46).background(.cyan.opacity(selected ? 0.14 : 0.06), in: RoundedRectangle(cornerRadius: 13))
            VStack(alignment: .leading, spacing: 4) {
                Text(name).font(.headline)
                Text(category).font(.caption).foregroundStyle(.secondary)
                Text(frequency).font(.caption2.monospaced()).foregroundStyle(.cyan)
            }
            Spacer()
            if selected { Text(isEnglish ? "FOLLOWING" : "СЛЕЖЕНИЕ").font(.system(size: 9, weight: .bold)).foregroundStyle(.green) }
            else { Image(systemName: "plus.circle").font(.title3).foregroundStyle(.secondary) }
        }.padding(13).background(.white.opacity(0.055), in: RoundedRectangle(cornerRadius: 16))
    }

    private var radar: some View {
        NavigationStack {
            VStack(spacing: 18) {
                pageTitle(isEnglish ? "Radar & compass" : "Радар и компас", subtitle: isEnglish ? "OBSERVER VIEW" : "ВИД НАБЛЮДАТЕЛЯ")
                    .frame(maxWidth: .infinity, alignment: .leading)
                ZStack {
                    Circle().fill(Color(red: 0.02, green: 0.12, blue: 0.19))
                    ForEach(1..<5) { i in Circle().stroke(.cyan.opacity(0.25), lineWidth: 1).padding(CGFloat(i) * 27) }
                    Path { p in p.move(to: CGPoint(x: 0, y: 150)); p.addLine(to: CGPoint(x: 300, y: 150)); p.move(to: CGPoint(x: 150, y: 0)); p.addLine(to: CGPoint(x: 150, y: 300)) }.stroke(.cyan.opacity(0.25), lineWidth: 1)
                    VStack { Text("N").foregroundStyle(.white); Spacer(); Text("S").foregroundStyle(.white) }
                    HStack { Text("W").foregroundStyle(.white); Spacer(); Text("E").foregroundStyle(.white) }
                    Circle().fill(.cyan).frame(width: 8, height: 8).shadow(color: .cyan, radius: 8).offset(x: 45, y: -60)
                    Image(systemName: "location.north.fill").font(.system(size: 24)).foregroundStyle(.cyan).offset(y: -2)
                }
                .frame(width: 300, height: 300).clipShape(Circle()).overlay(Circle().stroke(.cyan.opacity(0.55), lineWidth: 1))
                Text(isEnglish ? "Waiting for device compass and GPS" : "Ожидание компаса и GPS устройства").font(.caption).foregroundStyle(.secondary)
                Spacer()
            }.padding(18).toolbar(.hidden, for: .navigationBar)
        }
    }

    private var control: some View {
        NavigationStack {
            ScrollView {
                VStack(alignment: .leading, spacing: 20) {
                    pageTitle(isEnglish ? "Antenna rotator" : "Управление антенной", subtitle: "AZIMUTH / ELEVATION")
                    HStack {
                        Label(isEnglish ? "CYD device" : "Устройство CYD", systemImage: "antenna.radiowaves.left.and.right")
                        Spacer()
                        Text(isEnglish ? "OFFLINE" : "НЕ В СЕТИ").font(.caption2.bold()).foregroundStyle(.orange)
                    }.padding(14).background(.white.opacity(0.055), in: RoundedRectangle(cornerRadius: 14))
                    VStack(alignment: .leading, spacing: 12) {
                        HStack { Text(isEnglish ? "Azimuth" : "Азимут"); Spacer(); Text("\(Int(azimuth))°").monospacedDigit().foregroundStyle(.cyan) }
                        Slider(value: $azimuth, in: 0...360)
                        HStack { Text("0°"); Spacer(); Text("360°") }.font(.caption2).foregroundStyle(.secondary)
                    }.padding(16).background(.white.opacity(0.055), in: RoundedRectangle(cornerRadius: 16))
                    VStack(alignment: .leading, spacing: 12) {
                        HStack { Text(isEnglish ? "Elevation" : "Угол места"); Spacer(); Text("\(Int(elevation))°").monospacedDigit().foregroundStyle(.cyan) }
                        Slider(value: $elevation, in: 0...90)
                        HStack { Text("0°"); Spacer(); Text("90°") }.font(.caption2).foregroundStyle(.secondary)
                    }.padding(16).background(.white.opacity(0.055), in: RoundedRectangle(cornerRadius: 16))
                    Button { showDemoInfo = true } label: {
                        Label(isEnglish ? "Connect device" : "Подключить устройство", systemImage: "wifi")
                            .frame(maxWidth: .infinity).padding(14).background(.cyan.opacity(0.18), in: RoundedRectangle(cornerRadius: 14))
                    }.buttonStyle(.plain).foregroundStyle(.cyan)
                    Text(isEnglish ? "Sliders are preview controls only. They do not move motors." : "Ползунки пока только демонстрационные. Они не двигают моторы.")
                        .font(.caption).foregroundStyle(.secondary)
                }.padding(18)
            }.toolbar(.hidden, for: .navigationBar)
        }
    }

    private var settings: some View {
        NavigationStack {
            Form {
                Section(isEnglish ? "Appearance" : "Внешний вид") {
                    Picker(isEnglish ? "Language" : "Язык", selection: $language) {
                        Text("Русский").tag("ru")
                        Text("English").tag("en")
                    }
                }
                Section(isEnglish ? "Account & device" : "Аккаунт и устройство") {
                    Label(isEnglish ? "Cloud account — not connected" : "Облачный аккаунт — не подключён", systemImage: "person.crop.circle")
                    Label(isEnglish ? "CYD device — not paired" : "Устройство CYD — не привязано", systemImage: "cpu")
                }
                Section(isEnglish ? "About" : "О приложении") {
                    HStack { Text("CYD Satellite Tracker"); Spacer(); Text("0.1 • DEMO").foregroundStyle(.secondary) }
                    Text(isEnglish ? "Personal iPad edition" : "Персональная версия для iPad")
                }
            }
            .scrollContentBackground(.hidden)
            .background(Color.clear)
            .navigationTitle(isEnglish ? "Settings" : "Настройки")
        }
    }

    private func pageTitle(_ title: String, subtitle: String) -> some View {
        VStack(alignment: .leading, spacing: 5) {
            Text(title).font(.system(size: 27, weight: .bold, design: .rounded))
            Text(subtitle).font(.caption2.weight(.semibold)).tracking(1.8).foregroundStyle(.cyan)
        }.padding(.top, 12).padding(.bottom, 6)
    }
}

private struct SpaceBackground: View {
    var body: some View {
        LinearGradient(colors: [Color(red: 0.015, green: 0.035, blue: 0.085), Color(red: 0.025, green: 0.075, blue: 0.13), Color(red: 0.01, green: 0.025, blue: 0.06)], startPoint: .topLeading, endPoint: .bottomTrailing)
            .ignoresSafeArea()
            .overlay(alignment: .topTrailing) {
                Circle().fill(.cyan.opacity(0.07)).frame(width: 260).blur(radius: 70).offset(x: 90, y: -80)
            }
    }
}

private struct MetricCard: View {
    let title: String
    let value: String
    let detail: String
    let icon: String
    var body: some View {
        VStack(alignment: .leading, spacing: 9) {
            Label(title, systemImage: icon).font(.system(size: 9, weight: .bold)).tracking(1).foregroundStyle(.secondary)
            Text(value).font(.system(size: 29, weight: .bold, design: .rounded)).foregroundStyle(.white)
            Text(detail).font(.caption2).foregroundStyle(.secondary)
        }.frame(maxWidth: .infinity, alignment: .leading).padding(14)
            .background(.white.opacity(0.055), in: RoundedRectangle(cornerRadius: 16))
    }
}

private struct SectionHeading: View {
    let title: String
    let subtitle: String
    var body: some View {
        VStack(alignment: .leading, spacing: 3) {
            Text(title).font(.headline)
            Text(subtitle).font(.system(size: 9, weight: .semibold)).tracking(1.5).foregroundStyle(.cyan.opacity(0.8))
        }
    }
}

private struct OrbitIllustration: View {
    var body: some View {
        GeometryReader { geo in
            ZStack {
                LinearGradient(colors: [Color(red: 0.015, green: 0.05, blue: 0.13), Color(red: 0.015, green: 0.12, blue: 0.19)], startPoint: .top, endPoint: .bottom)
                ForEach(0..<35) { i in
                    Circle().fill(.white.opacity(i.isMultiple(of: 3) ? 0.7 : 0.35)).frame(width: i.isMultiple(of: 4) ? 2 : 1)
                        .position(x: CGFloat((i * 47 + 19) % 340), y: CGFloat((i * 29 + 13) % 210))
                }
                Ellipse().stroke(.cyan.opacity(0.45), style: StrokeStyle(lineWidth: 1, dash: [3, 5]))
                    .frame(width: geo.size.width * 0.9, height: 75).rotationEffect(.degrees(-17))
                Circle().fill(LinearGradient(colors: [Color(red: 0.08, green: 0.55, blue: 0.78), Color(red: 0.02, green: 0.15, blue: 0.38)], startPoint: .topLeading, endPoint: .bottomTrailing))
                    .frame(width: 145, height: 145).overlay(Circle().stroke(.cyan.opacity(0.5), lineWidth: 1))
                    .overlay(alignment: .topLeading) { Ellipse().fill(.green.opacity(0.35)).frame(width: 43, height: 18).rotationEffect(.degrees(-28)).offset(x: 24, y: 45) }
                    .shadow(color: .cyan.opacity(0.3), radius: 24).offset(y: 36)
                Image(systemName: "satellite.fill").font(.title2).foregroundStyle(.white).shadow(color: .cyan, radius: 8)
                    .offset(x: geo.size.width * 0.34, y: -48).rotationEffect(.degrees(-20))
                VStack { Spacer(); HStack { Text("ISS • DEMO ORBIT"); Spacer(); Text("3D VIEW") } }
                    .font(.system(size: 9, weight: .bold, design: .monospaced)).tracking(1).foregroundStyle(.cyan).padding(13)
            }
        }
    }
}
