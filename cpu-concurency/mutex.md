Mutex (Mutual Exclusion), çok thread'li programlamada **kritik bölüm** (critical section) koruması için kullanılan temel senkronizasyon primitifidir. İşte tam olarak nasıl çalıştığı:

## **1. Temel Kavram**

```cpp
std::mutex mtx;
int shared_data = 0;

void increment() {
    mtx.lock();      // 🔒 Giriş - Kritik bölüm başlangıcı
    shared_data++;   // 🎯 Kritik işlem (sadece bir thread)
    mtx.unlock();    // 🔓 Çıkış - Kritik bölüm sonu
}
```

## **2. Donanım Seviyesinde Nasıl Çalışır?**

### **Atomic Test-and-Set İşlemi**

Mutex'in kalbinde **atomik test-and-set** işlemi yatar:

```cpp
// Pseudocode - Gerçek mutex implementasyonu
class Mutex {
    std::atomic<bool> locked{false};
    
    void lock() {
        // BUSY-WAIT (spinlock): Bekleme döngüsü
        while (locked.exchange(true, std::memory_order_acquire)) {
            // CPU'yı boş yere yakma - pause/yield
            #ifdef __x86_64__
                __builtin_ia32_pause();  // CPU'ya beklemesini söyle
            #endif
            std::this_thread::yield();   // Diğer thread'lere şans ver
        }
    }
    
    void unlock() {
        locked.store(false, std::memory_order_release);
    }
}
```

### **Mutex Tipleri ve Performans:**

|Mutex Türü|Açıklama|Maliyet (lock/unlock)|Kullanım|
|---|---|---|---|
|**Normal**|Basic mutex|~25-50 ns (uncontended)|Genel kullanım|
|**Recursive**|Aynı thread tekrar lock alabilir|~2x normal|Recursive fonksiyonlar|
|**Adaptive**|Spin then sleep|Değişken|Kısa kritik bölümler|
|**PTHREAD_MUTEX_ERRORCHECK**|Hata kontrolü|~3x normal|Debug|
|**Spinlock**|Sadece busy-wait|~5-10 ns|Çok kısa kritik bölümler|


## **Modern C++'da Best Practices**

```cpp
// 1. RAII kullan (lock_guard, unique_lock)
{
    std::lock_guard<std::mutex> lock(mtx);  // Otomatik unlock
    // kritik bölüm
}  // lock otomatik serbest kalır

// 2. std::scoped_lock (C++17) - multiple mutex
std::mutex mtx1, mtx2;
{
    std::scoped_lock lock(mtx1, mtx2);  // Deadlock-free
    // çift mutex koruması
}

// 3. try_lock ile deadlock önleme
std::unique_lock<std::mutex> lock1(mtx1, std::defer_lock);
std::unique_lock<std::mutex> lock2(mtx2, std::defer_lock);
std::lock(lock1, lock2);  // All-or-nothing locking

// 4. Shared mutex (C++14) - read/write lock
std::shared_mutex rw_mutex;
{
    std::shared_lock lock(rw_mutex);  // Multiple readers
    // read-only işlem
}
{
    std::unique_lock lock(rw_mutex);  // Single writer
    // write işlem
}
```

### Advanced Knowledge
Mutex eger contented ise kernel call yapar, eger degilse sadece spinlock yapip kisa bir sure icerisinde doner.
```cpp
// Pseudocode - mutex.lock() gerçek implementasyonu
bool lock_fast_path() {
    // 1. Atomic compare-and-swap dene
    // 2. Eğer mutex serbestse (locked == 0):
    //    - locked = 1 yap
    //    - thread_id = current_thread set et
    //    - HEMEN DÖN (kernel çağrısı YOK)
    // 3. Maliyet: ~20-50 ns (sadece userspace)
    
    // Assembly'de:
    // lock cmpxchg [mutex], 1  // Atomic işlem
    // jz success              // Başarılıysa atla
    // call slow_path          // Başarısızsa slow path
}
```

Contended scenario
```cpp
bool lock_slow_path() {
    // 1. Mutex locked durumda
    // 2. Spin (bekle) - adaptive mutex'te kısa süre
    // 3. Hala alamadıysan KERNEL'e git:
    
    // Linux'ta:
    syscall(SYS_futex, &mutex, FUTEX_WAIT, expected_value, timeout);
    // ⬆️ BU KERNEL ÇAĞRISI!
    
    // Windows'ta:
    WaitForSingleObject(mutex_handle, INFINITE);
    // ⬆️ BU DA KERNEL ÇAĞRISI!
    
    // 4. Maliyet: ~1000-5000 ns (context switch + scheduling)
}
```

## **Visual Örnek: Thread'lerin Yolculuğu**

```text
THREAD A (İlk gelen)           THREAD B (İkinci gelen)
=============                  =============

[FAST PATH]
1. mutex.lock() çağır         1. mutex.lock() çağır
2. locked == 0 gör           2. locked == 1 gör (A lock'lamış)
3. locked = 1 yap            3. FAST PATH BAŞARISIZ
4. ✅ Kritik bölüme gir       4. ↓↓↓ SLOW PATH'e geç ↓↓↓
5. İşini yap
6. mutex.unlock() çağır      [SLOW PATH - KERNEL]
7. locked = 0 yap            5. Adaptive spin (userspace)
8. ⚡ futex_wake() çağır      6. Spin başarısız
                             7. ⚡ futex_wait() KERNEL ÇAĞRISI
                             8. ⏸️ Kernel'da UYUTULDU
                             9. ... (A thread'i bekler)
                             
                             10. ⏰ A unlock edince UYANDIRILDI
                             11. locked = 1 yapmayı dene
                             12. ✅ Kritik bölüme gir
```
## **Kernel Call'un Maliyeti Neden Yüksek?**

```cpp
// Kernel çağrısının adımları:
void kernel_context_switch() {
    // 1. User → Kernel mode geçiş (CPU ring 3 → ring 0)
    // 2. Register'ları kaydet
    // 3. Stack değiştir
    // 4. Memory mapping değiştir (kernel space)
    // 5. Kernel fonksiyonunu çalıştır
    // 6. Thread'i wait queue'ya ekle
    // 7. Schedule ediciyi çalıştır (başka thread'e geç)
    // 8. Uyandığında: Kernel → User mode geçiş
    // 9. Register'ları geri yükle
    // 10. Kaldığın yerden devam et
    
    // Toplam: 1000-5000 CPU cycle
    // (~300-1500 ns modern CPU'da)
}
```
Baska bir sonuc ise sirf kernel call yaptigi icin HFT lerde pek sevilmez.

