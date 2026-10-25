# İkinci güncelleme: dorse bağlama ve kabul

Hedef EXE aynı SHA-256 kapılı 1.61.1.9 derlemesidir. Bu kayıt statik inceleme
ve yerel test kanıtıdır; fiziksel bağlama veya Convoy başarısı iddiası değildir.

- Dorse Yöneticisi onayı `14123ad70` → `141237d60` → `1405f9840`.
  Yönetici üst katmanı garaja konumlandırma ve zaman ilerletme de yapar.
  Eklenti yalnız alt katmandaki dorse değiştirmeyi çağırır.
- `1405f9840(player,economy,index,trailer,placements,coupled)` gerçek sahiplik
  referanslarını dengeler, piyasa tekliflerini yeniler ve aktif fiziksel
  dorsenin değişimi için `1405f8a30` kullanır. Alt çağrının beş parametresi
  disassembly'de `1405f991d..9930` ile doğrulandı; decompiler eksik gösteriyordu.
- `1405f8a30` mevcut çekicinin konumunu alıp `1405dbf00` ile fiziksel dorseyi
  oluşturur. Boş yerleşim dizisi bu işlevde açıkça desteklenen yoldur;
  çekici konumundan dorse/zincir konumlarını üretir. `coupled=true`,
  `1405dcc20` native bağlantısını çağırır. Mevcut fiziksel çekici önkoşuldur.
- `1405f9fe0` sahip olunan araç kipini tamamlar. İki dorse erişim yolu
  tutarlı hale geldikten sonra seçilen şirket yeniden üretilir.
- `1405fb390` çalışanların kullandığı dorseyi arar; bu durumda otomatik
  değiştirme reddedilir. Aktif teslimat da iptal edilmez.
- Kabul öncesi `14106eb60` özgün diyalog kontrolü korunur. Devam eden provider
  görevi varsa değiştirme yapılmaz. Yalnız profilin yeniden okunmuş sahiplik
  listesinde bulunan dorse kabul edilir.
- Seçili satırın geçici teklifi, aynı bitiş zamanı, seed, yük, varış, mesafe,
  miktar ve doluluk oranına sahip gerçek şirket teklifiyle dengeli referans
  değişimi yapılarak değiştirilir. Dolgu baytları karşılaştırılmaz.
  Eşleşme yoksa farklı iş kabul edilmez; dorse bağlandığı belirtilerek tekrar
  seçim istenir. Native şirket seed tüketimi böylece özgün yolu kullanır.
- `1411a2c80` normal kabul işlevi korunur: genel piyasada `140600530` rota ve
  planlanan işi oluşturur; şirket piyasasında `14079e1e0` işi başlatır.
  Her iki job türünün vtable+60'ı `140845670` olup teklif alanlarını kopyalar;
  şirket/cargo referansları korunur, geçici teklif işaretçisi saklanmaz.
- Kaynaklar `.work/native-batch/` altında ilgili adresle adlandırılmıştır.
  Testler farklı yük miktarı, varış, seed ve doluluk oranını reddeder;
  farklı nesne başlığı/dolgu aynı işi bozmaz. Hatalı bellek ve sahip olunmayan
  dorse native çağrıdan önce reddedilir.

Sonraki kullanıcı testi: iş kabulünde gerekli dorsenin bağlandığı ve arayüzün
düzeldiği bildirildi. Ayrıntılar [doğrulama durumunda](VALIDATION.md).
Tekrar açma stres testi ve Convoy'da diğer oyuncunun gördüğü dorse ayrıca
doğrulanmalıdır.
