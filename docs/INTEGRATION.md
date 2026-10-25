# Entegrasyon kaydı — 25 Ekim 2026

Kullanıcı .scs + oyun eklentisi kurulumunu onayladı. Hedef kendi dorseleriyle
çalışılan Kargo/Yük Piyasası, tek oyunculu ve Convoy. Kullanıcının sonraki
isteğiyle otomatik dorse değiştirme ikinci güncellemeye eklendi.
[Güncel kabul bağlantısı](native-acceptance.md) ve [doğrulama durumu](VALIDATION.md)
bu ilk incelemedeki eski test durumlarını günceller. Bu belge tamamlanmış
veya Convoy'da doğrulanmış sürüm iddiası değildir.

## Hedef ve gözlemler

- Windows x64 ETS2 1.61.1.9; EXE boyutu 53.308.304 bayt.
- EXE SHA-256: b70c2357cc56eb5834ee47fea3c60b2f2d4591383ff2ac479cfd3cd254301b8c.
- base_share.scs/ui/cargo_market.sii SHA-256: 4120fa091af99c7fb047f91758aa9bfd9e334b0ab1a8177bbcb8f6e6eb63eebd.
- Kullanıcının zaten açık oyunundan salt okunur sorgu 10 sahip olunan dorseyi ve bir bağlı dorseyi döndürdü.
- Bu sorgu yerel UI veya teklif çağrılarının çalıştığını doğrulamaz.
- Ek salt okunur preflight: 66 çekici aksesuarının tamamında geçerli cache,
  1681 şirketin 691 uygun olanında en az 10 seed ve beklenen teklif dizisi tipi,
  bütün şirketlerde hazır şehir cache'i ve beklenen default scheduler görüldü.
  Kayıt: .work/runtime-preflight.json; hiçbir motor işlevi çağrılmadı.
- Oyun kapatılmadı, kayıt değiştirilmedi ve paket henüz kurulmadı.

## Uygulanan parçalar

| Parça | Durum / kanıt |
|---|---|
| Yeni seçici | Kontrol9470, x154..474/y810..840; native btn.listbox.sorting. Orijinal bloklar korunuyor; 6 paket testi geçti. |
| Sahiplik | Native alan okuyucu; 15 yapay bellek kontrolü ve canlı 10-dorse sonucu. |
| Plugin host | Resmi SDK1.16, DLL exportları, exact EXE kapısı, ana thread ve günlük; yanlış EXE gerçek DLL yükleme testinde reddedildi. |
| Hook sahipliği | Beklenen işaretçiye atomik değişim; başkasının işaretçisini ezmeden geri alma; 38 bellek kontrolü. |
| Teklif üretimi | Ekonomi/player/vehicle/company kopyaları; iki aktif-dorse erişimi kopyada yönlendirilir. Native motor çağrıları henüz çalıştırılmadı. |
| Sonuç satırları | Native satır, arama/tür/şehir/expiry ve ücret hesabı; ardından normal yayınlama/sıralama yolu. |
| Kabul / temizleme | Native kabul sanal işlevi iki giriş yolunu kapsar; teardown önce görevi iptal edip bekler ve satırları yok eder, sonra batch temizlenir. |
| Derleme | MSVC /W4 /WX; 5 CTest + 8 Node testi geçti. |
| Oyun içi / Convoy | Bekliyor. |

## Native sözleşmeler ve sınırlar

Üretici RVA 0x7cfba0 şirketin +0xa0 sahipli teklif dizisini temizler ve
yenisini doldurur. Normal yol ekonomi→player→ilk vehicle→dorseyi; fallback
player+0x98/+0xa0 üzerinden atanmış vehicle→dorseyi okur. Kopyada her iki yol
aynı seçili dorseye yönlendirilir. Gerçek ekonomi singleton'ı ve bağlı dorse
işaretçisi değiştirilmez. Şirket seed dizisi okunur; gerçek seed refill çağrılmaz.

Motor bazı metadata önbelleklerini tembel doldurur. Şirket şehir önbellekleri
orijinal şirketlerde native çözümleyici ile hazırlanır; sonra kopyalanır.
Çekici aksesuar önbelleği eksikse kod gerçek aksesuar metnini değiştirebildiği
için preflight bu durumda üretimi reddeder. Dolaylı sanal getter'ların
tüm yan etkileri kanıtlanmış değildir; hiçbir bellek yazımı yok iddiası yoktur.

Her kopyanın yalnızca bağımsız sahipli teklif dizisi yıkılır; tüm kopyaya native
destructor çağrılmaz. Üretilen teklifler ekranın satırları yok edilene kadar
yaşar. Profil verisine kaydedilmez ve bağlı olmayan dorseye ait satır kabul
edilmez. Bağlı dorse için özgün teklifler kullanılır.

Provider execute callback'i dönmeden default scheduler state=3 yayınlamaz.
Default scheduler işlevleri çalışma anında doğrulanır. Filtre değişimi native
yenileme bayrağını kullanır; özgün sıralama ölçütü değiştirilmez.

DLL hooklar görünür olmadan önce süreç ömrü boyunca sabitlenir. Başlatma
hatasında geri alma başarısız olsa bile callback kodu kaldırılmaz.
Kapanışta engine nesneleri hâlâ yaşıyorsa kuşkulu destructor çağrıları yerine
süreç ömrü boyunca tutulur. Başka eklentinin devraldığı slot ezilmez.

## İlk runtime testinde doğrulanacaklar

1. UI init/event ABI, 11 açılır liste girdisi ve Türkçe metin görünümü.
2. Gerçek company seed/cache yapıları ve generator çağrı ortamı.
3. Bağlı dorse listesiyle vanilla eşitliği; farklı dorselerde miktar/ücret/yük.
4. Arama/tür/şehir filtreleri ile fiyat/mesafe sıralamasının birlikte çalışması.
5. Hızlı seçim, ekran kapatma/açma ve profile dönmede nesne ömürleri.
6. Yanlış dorseye ait işi butonla/çift tıklamayla kabul etmeme.
7. Convoy katılma, piyasa yenileme, oturumdan çıkış ve eklenti çakışmaları.

## Kanıt kayıtları

REA/Ghidra çıktılarını .work/rea-evidence.json saklar. Bunlar özgün kaynak
kod değil; alan/ABI çıkarımları disassembly ile desteklenmiştir.

- Üretici: ev_a295eb68808c63aa26b5d0e6ad2cf59f373cab7286ddc2316521f97c72035087.
- Aday yükler: ev_2765ef44fbe77c6394a6b2f459866f933f4a21523c8e5830bdbe1107afbd0959.
- Yük birimleri: ev_05eff4e17197727849c45e7ae11581cf71f42fd850a3b2fb073e602a5e68a733.
- Yeni decompile .work/native-batch/, makine kodu .work/ui-binding/, canlı sahiplik .work/runtime-ownership.json.

Başvuru kaynakları: [SCS SDK](https://modding.scssoft.com/wiki/Documentation/Engine/SDK/Telemetry),
[SCS mod yöneticisi](https://modding.scssoft.com/wiki/Documentation/Engine/Mod_manager),
[1.32 ekonomi](https://modding.scssoft.com/wiki/Games/ETS2/Modding_guides/1.32#Economy_system_changes),
[1.54 yük sistemi](https://modding.scssoft.com/wiki/Games/ETS2/Modding_guides/1.54).
mp_mod_optional Convoy çalışma testi yerine geçmez.
