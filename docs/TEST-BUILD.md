# ETS2 Cargo Filter — deneysel test derlemesi

Hedef: Windows x64 ETS2 **1.61.1.9**, EXE SHA-256
`b70c2357cc56eb5834ee47fea3c60b2f2d4591383ff2ac479cfd3cd254301b8c`.
Başka sürümde eklenti devreye girmez.

Listeleme oyun içinde gözlendi; kullanıcı dorse seçimi, iş seçimi ve fiyat
tutarlılığını doğruladı. İkinci güncellemede arayüz düzeltmeleri ve iş kabulünde
otomatik dorse bağlama da kullanıcı tarafından oyun içinde doğrulandı.
Convoy testi henüz tamamlanmadı. Kurulum öncesi profil ve kayıt yedeği alın.

## Kurulum

1. `ETS2CargoFilter.dll` dosyasını oyun klasöründeki `bin/win_x64/plugins/`
   dizinine yerleştirin. Diğer eklentileri değiştirmeyin.
2. `ETS2CargoFilter.scs` dosyasını ETS2 belgeler klasörünün `mod/` dizinine
   yerleştirin ve oyunun Mod Yöneticisi'nden etkinleştirin.
3. Kendi dorselerinizle çalışılan Kargo/Yük Piyasası'nı açın. Yeni seçim
   mevcut yük türü filtresinin yanında görünür. Başlangıç seçimi tüm dorselerdir.

Bağlı dorse için oyunun mevcut teklifleri kullanılır. Diğer dorselerin
teklifleri motor üzerinden ayrı hesaplanır. İşi onaylarken ilgili dorse,
motorun fiziksel dorse oluşturma/bağlama yolu ile çekiciye bağlanır. Açılır
listede yalnız filtre değiştirmek dorseyi değiştirmez. Ardından oyunun normal
kabul akışı sürer: genel piyasada yükleme yerine rota, firma içindeki piyasada
iş kabulü. Uzaktaki bir yükü bulunduğunuz yere taşımaz.
Mevcut arama, tür filtresi, şehir kısıtı ve sıralama yolları kullanılır.

Dorse adları tür + plaka biçimindedir. Açılır liste satır sayısına göre kısalır;
çok fazla dorse varsa mevcut maksimum yükseklikte kaydırılır.
Filtre metinleri ve Mod Yöneticisi açıklaması oyunun seçili dilini izler;
43 dil/bölge seçeneği desteklenir. Dorse türleri oyunun kendi çevirilerini kullanır.
Yeni dil dosyaları nedeniyle güncellerken hem DLL hem de .scs dosyasını değiştirin.

## İlk oyun testi

- Açılışta eklenti hatası/çökme olmadığını ve `plugins/ETS2CargoFilter.log`
  dosyasındaki sürüm kontrolünün geçtiğini doğrulayın.
- Yeni seçimde profilin bütün dorselerinin ve plakalarının göründüğünü kontrol edin.
- Bağlı dorse seçiminin vanilla iş listesiyle eşleştiğini kontrol edin.
- Farklı gövde türünde iki dorseyi ayrı ayrı, ardından tüm dorseleri seçin.
- Fiyat/mesafe sıralaması, arama, yük türü filtresi ve şehir seçimini deneyin.
- Bağlı olmayan dorse işini onaylayın; bağlı dorseyi ve seçilen yük/ücreti kontrol edin.
- Piyasayı açıp kapatın, hızlı filtre değişiklikleri yapın, profil menüsüne dönün.
- Ardından Convoy oturumunda aynı kontrolleri ve oturuma katılma/çıkmayı deneyin.

`mp_mod_optional: true` yalnızca arayüz paketinin isteğe bağlı olduğunu bildirir;
Convoy davranışının test edildiği anlamına gelmez.

## Kaldırma

Oyunu kapatın. Mod Yöneticisi'nde paketi kapatın; yalnızca yukarıdaki iki
dosyayı kaldırın. Mevcut diğer eklentileri veya profil dosyalarını silmeyin.

## Bilinen geliştirme sınırları

- Aynı işi birden fazla dorse taşıyabiliyorsa tüm-dorseler listesinde birden
  fazla satır görülebilir; miktar ve ücret farklılıkları korunur.
- Dorse değiştirme/satış sonrası piyasayı yeniden açın.
- Başka işte kullanılan dorse otomatik alınmaz. Teklif eşleşmesi değişmişse
  dorse bağlandıktan sonra yeniden seçim istenir; farklı yük sessizce alınmaz.
- Uzun açık kalan ekranda hesaplama önbelleği sınıra ulaşırsa yeniden açma
  mesajı gösterilir. Teklif üretimi ilk açılışta bekleme yaratabilir.
- Başka bir arayüz modu `ui/cargo_market.sii` dosyasını değiştirdiğinde paket
  önceliği çakışabilir. Aynı native işlevleri değiştiren eklentide başlatma reddedilir.
