# ETS2 Cargo Filter

Euro Truck Simulator 2 Kargo/Yük Piyasası için sahip olunan dorse filtresi.
Mevcut yük türü filtresinin yanına, oyunun görsel stilini kullanan bir dorse
seçicisi ekler. Windows x64 eklentisi ve `.scs` arayüz paketinden oluşur.

## İndir ve kur

[Hazır kurulum paketleri](https://github.com/metinyildiran/ETS2CargoFilter/releases)

[Steam Atölyesi](https://steamcommunity.com/sharedfiles/filedetails/?id=3827220762)
üzerinden arayüze abone olabilirsiniz. **Atölye DLL kurmaz:** aşağıdaki
eklenti kurulumunu ayrıca yapın. Atölye kullanırken elle kurulmuş `.scs`
kopyasını etkinleştirmeyin; iki arayüz paketinden yalnız birini kullanın.

**Desteklenen sürüm: ETS2 1.61.1.9, Windows 64-bit.** Eklenti tam EXE
SHA-256 kontrolü yapar; farklı oyun derlemesinde etkinleşmez.

1. Oyunu kapatın ve profil/kayıtlarınızın yedeğini alın.
2. ZIP içindeki `ETS2CargoFilter.dll` dosyasını oyun klasörünün
   `bin/win_x64/plugins` dizinine kopyalayın. Klasör yoksa oluşturun.
3. `ETS2CargoFilter.scs` dosyasını `Belgeler/Euro Truck Simulator 2/mod`
   dizinine kopyalayın. Belgeler OneDrive'a taşınmışsa oradaki ETS2 dizinini kullanın.
4. Mod Yöneticisi'nde **ETS2 Cargo Filter** modunu
   etkinleştirin ve kariyerinize girin.
5. Kendi dorselerinizle çalışılan Kargo/Yük Piyasası'nı açın.

Steam'de oyuna sağ tıklayıp **Yönet → Yerel dosyalara göz at** seçeneğiyle
oyun klasörünü bulabilirsiniz. `.scs` tek başına yeterli değildir.

## Özellikler

- Profildeki bütün dorselere uygun teklifler veya tek dorseye göre filtreleme.
- Kendi çekicisi ve en az bir dorsesi olan profillerde, dorse atanmamış olsa da
  Kargo Piyasası düğmesine erişim.
- Dorse türü + plaka etiketleri ve bağlı dorse işareti.
- Oyunun seçili dilini izleyen 43 dil/bölge desteği; filtre, durum mesajları ve
  Mod Yöneticisi açıklaması çevrilidir. Dorse türleri oyunun kendi çevirilerini kullanır.
- Satır sayısına göre açılır liste yüksekliği; uzun listelerde kaydırma.
- Oyunun mevcut iş listesi, sıralaması, arama ve yük türü filtresi.
- İşi onaylayınca ilgili dorseyi native motor üzerinden otomatik bağlama.
- Genel piyasada normal yükleme yeri rotası, firma piyasasında normal iş kabulü.

Filtre seçmek yalnızca listeyi değiştirir. Dorse, iş onaylandığında bağlanır.
Aynı yükü farklı dorseler taşıyabiliyorsa miktar/ücret farkları ayrı satırlarda
korunur. Kullanımdaki bir çalışan dorsesi otomatik alınmaz; teklif değişmişse
farklı bir işi kabul etmek yerine yeniden seçim istenir.

## Durum ve uyumluluk

İkinci deneysel sürümde listeleme, iş seçimi, fiyat tutarlılığı, arayüz
boyutlandırması ve otomatik dorse bağlama kullanıcı tarafından oyun içinde
doğrulandı. **Convoy oturum testi henüz tamamlanmadı.** Filtreyi kullanmak
isteyen her oyuncu eklentiyi kendi bilgisayarına kurmalıdır.

Arayüz ve eklenti bu yayında `1.61.1.9` sürümüyle sınırlandırılmıştır.
`mp_mod_optional` ayarı Convoy'da doğrulandığı anlamına gelmez.
Başka bir mod `ui/cargo_market.sii` dosyasını değiştiriyorsa öncelik çakışabilir.

## Kaynaktan derleme

Visual Studio 2022 C++ masaüstü geliştirme araçları, CMake ve Node.js gerekir.
SDK 1.16 başlıkları kendi lisansıyla `vendor/` altında bulunur.

```powershell
./tools/build-native.ps1
./tools/package-dev.ps1 -GameDirectory 'D:\SteamLibrary\steamapps\common\Euro Truck Simulator 2' -SkipBuild
node --test tests/hashfs.test.mjs tests/ui-package.test.mjs tests/localization.test.mjs
```

Arayüzü üretmek için desteklenen oyunun yerel kurulumu gerekir; oyun arşivleri
bu depoda bulunmaz. Farklı kurulum yolunda arayüz testleri için `ETS2_GAME_DIR`
ortam değişkenini ayarlayın. Oyun kurulumu yoksa kaynak bağımsız testler çalışır,
oyun kaynaklarını gerektiren arayüz testleri atlanır.

Çıktılar `dist/experimental/` altına yazılır. `install-dev.ps1` kullanılırsa
`-GameDirectory` ve yedeklenecek `-SteamProfileDirectory` açıkça verilmelidir.
Bu araç oyun kapalıyken yedek alır ve yalnız modun iki dosyasını kurar.

## Teknik belgeler

- [Kurulum ve kullanım ayrıntıları](docs/TEST-BUILD.md)
- [Doğrulama durumu](docs/VALIDATION.md)
- [Dil desteği ve çeviriler](docs/localization.md)
- [Atölye paketleme ve yayın](docs/WORKSHOP.md)
- [Native dorse bağlama ve kabul](docs/native-acceptance.md)
- [Sahiplik okuyucu](docs/ownership-binding.md)
- [Arayüz ve görev ömürleri](docs/ui-binding.md)
- [Üçüncü taraf bildirimleri](tools/THIRD_PARTY_NOTICES.md)

Oyun dosyaları, profil/kayıt yedekleri, indirilen analiz araçları ve yerel
analiz çıktıları kaynak depoya dahil değildir. Hazır paketler Releases
sayfasında dağıtılır. SCS Software tarafından geliştirilmiş veya onaylanmış
resmî bir eklenti değildir.
