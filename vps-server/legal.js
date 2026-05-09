// VoLaura - Web sayfaları içerikleri
// Tüm sayfalar shellHtml() içine sarılarak gösterilir.

const HOME_HTML = `
<div style="text-align:center;padding:8px 0 6px 0;">
  <img src="/logo.png" alt="VoLaura" width="140" height="140"
       style="width:140px;height:140px;object-fit:contain;
              filter:drop-shadow(0 14px 40px rgba(123,63,228,0.55));margin-bottom:8px;">
  <div style="color:#fff;font-size:38px;font-weight:800;letter-spacing:1.4px;margin:6px 0 2px 0;
              background:linear-gradient(135deg,#7fb3ff 0%,#c89bff 100%);
              -webkit-background-clip:text;-webkit-text-fill-color:transparent;background-clip:text;">VoLaura</div>
  <h2 style="margin:8px 0 4px 0;color:#f4f7ff;font-size:26px;font-weight:700;">Sesin, görüntün, tek yerde.</h2>
  <p style="color:#9fb2dc;font-size:15px;max-width:580px;margin:8px auto 18px auto;line-height:1.6;">
    VoLaura; arkadaşlarınla anlık sesli ve görüntülü görüşme,
    sunucu/kanal bazlı sohbet ve ekran paylaşımı sunan, gizliliğe öncelik veren bir iletişim platformudur.
  </p>
</div>

<div style="display:grid;grid-template-columns:repeat(auto-fit,minmax(220px,1fr));gap:14px;margin:18px 0 6px 0;">
  <div style="background:#0a0e16;border:1px solid #1f2636;border-radius:12px;padding:16px 18px;">
    <div style="color:#7fb3ff;font-weight:700;margin-bottom:6px;">⚡ Düşük Gecikme</div>
    <div style="color:#9fb2dc;font-size:13px;">Sesli sohbette akıcı, kesintisiz iletişim.</div>
  </div>
  <div style="background:#0a0e16;border:1px solid #1f2636;border-radius:12px;padding:16px 18px;">
    <div style="color:#7fb3ff;font-weight:700;margin-bottom:6px;">🔒 Güvenli</div>
    <div style="color:#9fb2dc;font-size:13px;">2FA, TOTP, SMS ve e-posta tabanlı doğrulama.</div>
  </div>
  <div style="background:#0a0e16;border:1px solid #1f2636;border-radius:12px;padding:16px 18px;">
    <div style="color:#7fb3ff;font-weight:700;margin-bottom:6px;">📺 Ekran Paylaşımı</div>
    <div style="color:#9fb2dc;font-size:13px;">Tek tıkla pencere veya ekran paylaş.</div>
  </div>
  <div style="background:#0a0e16;border:1px solid #1f2636;border-radius:12px;padding:16px 18px;">
    <div style="color:#7fb3ff;font-weight:700;margin-bottom:6px;">🎙️ Yüksek Kalite</div>
    <div style="color:#9fb2dc;font-size:13px;">Net ses, akıcı kamera akışları.</div>
  </div>
</div>

<hr>

<div style="display:flex;flex-wrap:wrap;gap:14px;margin-top:6px;">
  <a href="/terms" style="flex:1;min-width:200px;background:#0a0e16;border:1px solid #1f2636;
       border-radius:12px;padding:14px 16px;text-decoration:none;display:block;">
    <div style="color:#cdd7ef;font-weight:700;margin-bottom:4px;">Hizmet Şartları →</div>
    <div style="color:#7d8ba7;font-size:12.5px;">Kullanım koşulları, kabul edilebilir davranışlar.</div>
  </a>
  <a href="/privacy" style="flex:1;min-width:200px;background:#0a0e16;border:1px solid #1f2636;
       border-radius:12px;padding:14px 16px;text-decoration:none;display:block;">
    <div style="color:#cdd7ef;font-weight:700;margin-bottom:4px;">Gizlilik Politikası →</div>
    <div style="color:#7d8ba7;font-size:12.5px;">Veri toplama, saklama ve haklarınız.</div>
  </a>
</div>

<p style="color:#7d8ba7;font-size:12px;margin-top:22px;text-align:center;">
  Sorular için <a href="mailto:support@volaura.xyz" style="color:#7fb3ff;">support@volaura.xyz</a>.
</p>
`;

// =====================================================================
//                       HİZMET ŞARTLARI (TERMS)
// =====================================================================
const TERMS_HTML = `
<h2>Hizmet Şartları</h2>
<p class="meta">Sürüm 1.2.0 &middot; Yürürlük: 3 Mayıs 2026 &middot; Son güncelleme: 3 Mayıs 2026</p>

<p>İşbu Hizmet Şartları (kısaca <b>"Şartlar"</b>), <b>VoLaura</b> ("Hizmet", "Platform", "biz") tarafından
sunulan masaüstü uygulamasının ve <a href="https://volaura.xyz">volaura.xyz</a> üzerindeki ilgili web hizmetlerinin
kullanımına ilişkin koşulları düzenler. Hizmeti kullanarak bu Şartları kabul ettiğinizi beyan edersiniz.
Şartları kabul etmiyorsanız Hizmeti kullanmamanız gerekir.</p>

<h3>1. Hizmet Tanımı</h3>
<p>VoLaura; gerçek zamanlı sesli ve görüntülü iletişim, metin tabanlı sohbet, dosya paylaşımı,
ekran ve kamera yayını, sunucu/kanal yönetimi ve arkadaş listesi gibi özellikleri içeren bir
iletişim ve sosyal etkileşim hizmetidir. Hizmet "olduğu gibi" sağlanır ve önceden bildirim
yapılmaksızın değiştirilebilir, geliştirilebilir veya kısmen/tamamen durdurulabilir.</p>

<h3>2. Hesap, Kayıt ve Yaş Sınırı</h3>
<ul>
  <li>Hizmetten faydalanabilmek için <b>en az 13 yaşında</b> olmanız gerekir.
      AB ülkelerinde ilgili ulusal mevzuatın belirlediği daha yüksek yaş sınırları uygulanır.</li>
  <li>Hesap oluştururken doğru ve güncel bilgi vermeyi, e-posta adresinizi ve (eklerseniz)
      telefon numaranızı sahipliği size ait olduğu sürece kullanmayı kabul edersiniz.</li>
  <li>Hesap güvenliğinizden tamamen siz sorumlusunuz; şifrenizi paylaşmamak, güvenli bir
      yerde saklamak ve mümkünse iki adımlı doğrulamayı (2FA) açmakla yükümlüsünüz.</li>
  <li>Sahte kimlik kullanımı, başkasının kimliğine bürünme, hesap satışı/devri ve birden fazla
      hesapla şartları aşmaya çalışmak yasaktır.</li>
</ul>

<h3>3. Kabul Edilemez Davranışlar</h3>
<p>Aşağıdaki davranışlar Hizmette kesinlikle yasaktır ve hesabınızın askıya alınmasıyla sonuçlanabilir:</p>
<ul>
  <li>Tehdit, taciz, zorbalık, nefret söylemi, ayrımcılık ve cinsiyetçi/ırkçı/dini saldırı.</li>
  <li>Çocuk istismarı içeren ya da çocukları cinsel olarak tehlikeye atan her türlü içerik.</li>
  <li>Yasa dışı faaliyetlerin teşvik edilmesi (uyuşturucu, silah, dolandırıcılık, kara para vb.).</li>
  <li>Üçüncü kişilerin gizliliğini ihlal etmek; izinsiz görüşme kaydı, ses/görüntü yayını veya bu
      kayıtların başkalarıyla paylaşılması.</li>
  <li>Hizmete veya altyapısına yetkisiz erişim, tersine mühendislik, otomatik bot/script
      kullanımı, anti-spam ve hız sınırlarını aşma girişimleri.</li>
  <li>Telif hakkı, ticari marka veya diğer fikri mülkiyet haklarını ihlal eden materyallerin
      paylaşımı.</li>
  <li>Spam, istenmeyen reklam, zincir mesaj veya sahte/yanıltıcı içerik.</li>
  <li>Zararlı yazılım, bağlantı, exploit veya sosyal mühendislik girişimleri.</li>
</ul>

<h3>4. Kullanıcı İçeriği</h3>
<p>Hizmette paylaştığınız metin, ses, görüntü, dosya gibi tüm içerikler size aittir. Ancak
Hizmetin sağlanması (mesajların depolanması, dağıtılması, iletilmesi) amacıyla bu içeriklere
sınırlı, dünya çapında, telifsiz ve ücretsiz bir kullanım lisansı tanırsınız.</p>
<p>Sesli/görüntülü akış verileri sunucularımızda <b>kalıcı olarak saklanmaz</b>; akış sırasında
ilgili katılımcılara iletilir ve oturum sona erdiğinde sunucu hafızasından silinir. Yazılı mesajlar
(kanal mesajları, doğrudan mesajlar) hesabınız aktif olduğu sürece veritabanımızda tutulur.</p>

<h3>5. Doğrulama, SMS ve E-posta Kullanımı</h3>
<ul>
  <li>Kayıt sırasında verdiğiniz e-posta adresine bir doğrulama bağlantısı gönderilir.
      Hesabınızı kullanabilmek için bu bağlantıya tıklamanız gerekir.</li>
  <li>İsteğe bağlı olarak telefon numaranızı doğrulayabilir ve SMS ile iki adımlı doğrulamayı
      etkinleştirebilirsiniz. Bu durumda mesajlaşma operatör ücretleri size ait olabilir.</li>
  <li>E-posta gönderimi <b>SendGrid (Twilio Inc.)</b>, SMS gönderimi ise <b>Twilio</b> servisleri
      üzerinden gerçekleştirilir. Detaylar Gizlilik Politikasında belirtilmiştir.</li>
</ul>

<h3>6. Ücretler</h3>
<p>Bu Şartların yürürlüğe girdiği tarihte VoLaura'nın temel özellikleri ücretsizdir. Gelecekte
ücretli özellikler veya planlar eklenmesi durumunda bu Şartlar güncellenir ve sizden ek
onay/abonelik gerekmesi halinde bilgilendirme yapılır.</p>

<h3>7. Hesabın Askıya Alınması ve Sonlandırılması</h3>
<ul>
  <li>Bu Şartların ihlali, kötü niyetli kullanım, hizmete zarar verme girişimleri veya yetkili
      makamların talebi durumunda hesabınız geçici olarak askıya alınabilir veya kalıcı olarak
      kapatılabilir.</li>
  <li>Hesabınızı dilediğiniz zaman <a href="mailto:support@volaura.xyz">support@volaura.xyz</a>
      adresine yazarak silebilirsiniz. Silme talebinizden itibaren makul bir süre içinde kişisel
      verileriniz Gizlilik Politikasında belirtilen istisnalar saklı kalmak kaydıyla kaldırılır.</li>
</ul>

<h3>8. Garanti Reddi</h3>
<p>Hizmet "OLDUĞU GİBİ" ve "MEVCUT OLDUĞU SÜRECE" sunulmaktadır. Yasaların izin verdiği
azami ölçüde, kesintisizlik, hatasızlık, satılabilirlik veya belirli bir amaca uygunluk dahil
açık veya zımni hiçbir garanti verilmemektedir.</p>

<h3>9. Sorumluluğun Sınırlandırılması</h3>
<p>Yasaların izin verdiği azami ölçüde VoLaura geliştiricileri; Hizmetin kullanımından,
kullanılamamasından, veri kaybından, kâr kaybından veya tesadüfi/dolaylı/özel/cezai zararlardan
sorumlu tutulamaz. Sorumluluğumuzun toplam tutarı, ihtilafa konu olayın ortaya çıkmasından
önceki 12 ayda Hizmet için bize ödediğiniz toplam tutarı (ücretsiz hizmetlerde sıfır TL'yi)
aşamaz.</p>

<h3>10. Tazminat</h3>
<p>Bu Şartları ihlal etmeniz veya Hizmeti kötüye kullanmanız nedeniyle üçüncü kişilerin VoLaura'ya
karşı talepte bulunması halinde, VoLaura'yı bu tür taleplere karşı tazmin etmeyi ve savunmayı
kabul edersiniz.</p>

<h3>11. Şartlarda Değişiklik</h3>
<p>Bu Şartlar zaman zaman güncellenebilir. Önemli değişiklikler kayıtlı e-posta adresinize
duyurulur. Güncelleme sonrasında Hizmeti kullanmaya devam etmeniz, yeni Şartları kabul ettiğiniz
anlamına gelir.</p>

<h3>12. Uygulanacak Hukuk ve Yetkili Mahkeme</h3>
<p>Bu Şartlar Türkiye Cumhuriyeti yasalarına tabidir. Bu Şartlardan doğan ihtilaflarda
İstanbul Mahkemeleri ve İcra Daireleri yetkilidir. Tüketici sıfatınızla yasal koruma haklarınız
saklıdır.</p>

<h3>13. İletişim</h3>
<p>Şartlar hakkında her türlü soru için:
  <b>support@volaura.xyz</b>
  &middot; <a href="/privacy">Gizlilik Politikası</a>
</p>
`;

// =====================================================================
//                       GİZLİLİK POLİTİKASI
// =====================================================================
const PRIVACY_HTML = `
<h2>Gizlilik Politikası</h2>
<p class="meta">Sürüm 1.2.0 &middot; Yürürlük: 3 Mayıs 2026 &middot; Son güncelleme: 3 Mayıs 2026</p>

<p>Bu Gizlilik Politikası, <b>VoLaura</b>'nın hangi kişisel verileri topladığını, bu verileri
nasıl kullandığını, kimlerle paylaştığını, ne kadar süreyle sakladığını ve sahip olduğunuz
hakları açıklar. Politikayı dikkatle okumanızı rica ederiz.</p>

<h3>1. Veri Sorumlusu</h3>
<p>İşbu Politika kapsamında veri sorumlusu <b>VoLaura Proje Ekibi</b>'dir.
İletişim: <a href="mailto:support@volaura.xyz">support@volaura.xyz</a>.</p>

<h3>2. Topladığımız Kişisel Veriler</h3>
<p>Hizmeti sağlamak ve geliştirmek için aşağıdaki kategorilerde kişisel veri toplarız:</p>
<ul>
  <li><b>Hesap bilgileri:</b> kullanıcı adı, e-posta adresi, geri dönüştürülemez şekilde
      hash'lenmiş şifre, hesap oluşturma tarihi.</li>
  <li><b>İsteğe bağlı bilgiler:</b> telefon numarası (E.164 biçiminde), profil resmi (yüklerseniz),
      görünen ad/biyografi.</li>
  <li><b>Güvenlik verileri:</b> TOTP secret (yalnızca etkinleştirirseniz), SMS/e-posta tek kullanımlık
      kodları (kısa ömürlü, kullanıldıktan sonra silinir), 2FA durumu.</li>
  <li><b>Mesajlaşma içeriği:</b> kanal/DM yazılı mesajları, paylaştığınız dosyalar.</li>
  <li><b>Sesli/görüntülü akış verileri:</b> sunucularımızda <b>SAKLANMAZ</b>; oturum süresince
      doğrudan diğer katılımcılara iletilir.</li>
  <li><b>Cihaz ve bağlantı verileri:</b> IP adresi, oturum açma zamanları, kullanıcı agent bilgisi,
      yaklaşık coğrafi bölge (yalnızca güvenlik amaçlı).</li>
  <li><b>Günlükler:</b> hata kayıtları, güvenlik olayları, hız limit ihlalleri.</li>
</ul>

<h3>3. Verileri Kullanım Amaçlarımız</h3>
<ul>
  <li>Hesabınızı oluşturmak, kimlik doğrulamak ve giriş yapmanızı sağlamak.</li>
  <li>Hizmetin temel işlevlerini sunmak (sesli/görüntülü/yazılı iletişim, ekran paylaşımı).</li>
  <li>İki adımlı doğrulama, şifre sıfırlama ve hesap kurtarma süreçlerini yürütmek.</li>
  <li>Spam, dolandırıcılık ve istismarı tespit etmek; platform güvenliğini sağlamak.</li>
  <li>Hizmeti iyileştirmek, hata ayıklamak, performans analizi yapmak.</li>
  <li>Yasal yükümlülüklere uymak ve hak taleplerini değerlendirmek.</li>
</ul>

<h3>4. İşleme Hukuki Dayanağı (KVKK / GDPR)</h3>
<ul>
  <li><b>Sözleşmenin kurulması/ifası:</b> Hesap oluşturma, mesajlaşma, hizmetin sunulması.</li>
  <li><b>Açık rıza:</b> Telefon numarası ekleme, SMS/e-posta 2FA gibi isteğe bağlı işlemler.</li>
  <li><b>Meşru menfaat:</b> Spam ve kötü niyetli kullanımın önlenmesi, güvenlik günlükleri.</li>
  <li><b>Yasal yükümlülük:</b> Mahkeme kararı veya kanunun zorunlu kıldığı bilgi paylaşımı.</li>
</ul>

<h3>5. Üçüncü Taraf Servis Sağlayıcılar</h3>
<p>Aşağıdaki güvenilir alt yüklenicileri kullanırız. Bu şirketler yalnızca hizmeti sağlamak için
gereken minimum veriyi alır ve kendi gizlilik politikalarına bağlıdırlar:</p>
<ul>
  <li><b>SendGrid (Twilio Inc.)</b> &mdash; Doğrulama e-postası, şifre sıfırlama, giriş kodu gönderimi.
      <a href="https://www.twilio.com/legal/privacy" target="_blank">Gizlilik politikası</a>.</li>
  <li><b>Twilio</b> &mdash; SMS doğrulama kodu gönderimi (yalnızca telefon numarası eklerseniz).
      <a href="https://www.twilio.com/legal/privacy" target="_blank">Gizlilik politikası</a>.</li>
  <li><b>Neon (PostgreSQL)</b> &mdash; Veritabanı barındırma. Veriler şifreli aktarımla
      taşınır ve disk düzeyinde şifrelenmiş olarak saklanır.</li>
  <li><b>Let's Encrypt</b> &mdash; TLS sertifikaları. Yalnızca alan adı bilgisi paylaşılır.</li>
</ul>

<h3>6. Veri Saklama Süreleri</h3>
<table style="border-collapse:collapse;width:100%;margin-top:8px;">
  <tr style="border-bottom:1px solid #1f2636;">
    <th align="left" style="padding:8px 6px;color:#cdd7ef;">Veri Türü</th>
    <th align="left" style="padding:8px 6px;color:#cdd7ef;">Süre</th>
  </tr>
  <tr style="border-bottom:1px solid #1f2636;"><td style="padding:8px 6px;color:#9fb2dc;">Hesap bilgileri</td>
    <td style="padding:8px 6px;color:#9fb2dc;">Hesap aktif olduğu sürece</td></tr>
  <tr style="border-bottom:1px solid #1f2636;"><td style="padding:8px 6px;color:#9fb2dc;">Yazılı mesajlar</td>
    <td style="padding:8px 6px;color:#9fb2dc;">Hesap aktif olduğu sürece (silebilirsiniz)</td></tr>
  <tr style="border-bottom:1px solid #1f2636;"><td style="padding:8px 6px;color:#9fb2dc;">Sesli/görüntülü akış</td>
    <td style="padding:8px 6px;color:#9fb2dc;"><b>Saklanmaz</b> (oturum süresince hafızada)</td></tr>
  <tr style="border-bottom:1px solid #1f2636;"><td style="padding:8px 6px;color:#9fb2dc;">Doğrulama / 2FA kodları</td>
    <td style="padding:8px 6px;color:#9fb2dc;">5–10 dakika</td></tr>
  <tr style="border-bottom:1px solid #1f2636;"><td style="padding:8px 6px;color:#9fb2dc;">Şifre sıfırlama tokenleri</td>
    <td style="padding:8px 6px;color:#9fb2dc;">1 saat</td></tr>
  <tr><td style="padding:8px 6px;color:#9fb2dc;">Güvenlik günlükleri (IP/hata)</td>
    <td style="padding:8px 6px;color:#9fb2dc;">En fazla 90 gün</td></tr>
</table>

<h3>7. Veri Güvenliği</h3>
<ul>
  <li>Tüm veri aktarımı TLS 1.2/1.3 ile şifrelenir.</li>
  <li>Şifreler güçlü hash algoritmaları ile saklanır; düz metin halinde tutulmaz.</li>
  <li>İki adımlı doğrulama, oturum bazlı yetkilendirme ve oran sınırlamaları kullanılır.</li>
  <li>Erişim, ihtiyaç duyulan yetkili kişilerle sınırlıdır ve kayıt altına alınır.</li>
</ul>

<h3>8. Haklarınız (KVKK m.11 / GDPR m.15-22)</h3>
<p>Veri sorumlusu sıfatıyla bize başvurarak aşağıdaki haklarınızı kullanabilirsiniz:</p>
<ul>
  <li>Kişisel verilerinizin işlenip işlenmediğini öğrenme.</li>
  <li>İşleme amacını ve verilerinize erişim talep etme.</li>
  <li>Yanlış veya eksik verilerin düzeltilmesini isteme.</li>
  <li>Verilerinizin silinmesini veya yok edilmesini isteme.</li>
  <li>Veri taşınabilirliği talep etme (yapılandırılmış formatta dışa aktarım).</li>
  <li>İşlemeye itiraz etme, profilleme/otomatik karar alma süreçlerinden çıkma.</li>
  <li>Verilerinizin yurt dışına aktarımına ilişkin bilgi alma.</li>
</ul>
<p>Talepleriniz için <a href="mailto:support@volaura.xyz">support@volaura.xyz</a> adresine
yazabilirsiniz. Başvurularınız <b>30 gün</b> içinde değerlendirilir.</p>

<h3>9. Çocukların Gizliliği</h3>
<p>VoLaura 13 yaşın altındaki kullanıcılardan bilerek veri toplamaz. Çocuğunuzun bizden izinsiz
veri sağladığını fark ederseniz lütfen iletişime geçin; bilgileri derhal sileriz.</p>

<h3>10. Çerezler ve İzleme</h3>
<p>Web sayfalarımız (volaura.xyz) yalnızca işlevsel oturum çerezleri kullanabilir. Reklam veya
üçüncü taraf izleme çerezleri bulundurmuyoruz. Masaüstü uygulaması yerel cihazınızda yapılandırma
verilerini saklamak için yerel ayar dosyaları kullanır.</p>

<h3>11. Veri Yurt Dışına Aktarım</h3>
<p>Kullandığımız bazı hizmet sağlayıcılar (Twilio, SendGrid, Neon) verileri AB veya ABD'de barındırır.
Bu aktarımlar yeterlilik kararı veya standart sözleşme maddeleri çerçevesinde gerçekleşir.</p>

<h3>12. Veri İhlali Bildirimi</h3>
<p>Kişisel verilerinizi etkileyen bir güvenlik ihlali tespit edersek, ilgili mevzuatın gerektirdiği
süreler içinde sizi ve denetim kurumlarını bilgilendiririz.</p>

<h3>13. Politikada Değişiklik</h3>
<p>Bu Politikada yapılacak değişiklikler bu sayfada yayımlanır. Önemli değişiklikler kayıtlı
e-posta adresinize bildirilir. Değişiklik sonrası Hizmeti kullanmaya devam etmeniz, güncel
Politikayı kabul ettiğiniz anlamına gelir.</p>

<h3>14. İletişim</h3>
<p>Gizlilik konusundaki sorularınız ve hak başvurularınız için:
  <b>support@volaura.xyz</b>
  &middot; <a href="/terms">Hizmet Şartları</a>
</p>
`;

module.exports = { HOME_HTML, TERMS_HTML, PRIVACY_HTML };
