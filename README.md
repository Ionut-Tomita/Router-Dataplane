# 🛰️ Router Dataplane — Tema PCOM

**Autor:** Tomița Ionuț  
**Grupă:** 325CDa  
**Descriere:**  
În cadrul acestei teme am avut de implementat *dataplane*-ul unui router, simulând procesele de dirijare și tratare a pachetelor la nivel IP.

---

## ⚙️ Implementare

Am început implementarea temei folosindu-mă de logica laboratorului 4.  
Codul este structurat modular, cu funcții clare pentru fiecare etapă a procesării pachetelor.

---

## 📡 Procesul de Dirijare

Procesul de dirijare a fost realizat folosind tabela statică de ARP din fișierul  
`arp_table.txt`.

1. **Inițializare:**  
   - Se încarcă tabela ARP și tabela de rutare.  
   - Se așteaptă primirea pachetelor de la interfețe.

2. **Verificare tip pachet:**  
   - Dacă pachetul nu este de tip **IPv4**, acesta este aruncat (*drop*).  
   - Dacă destinatarul este chiar routerul, se generează un mesaj **ICMP Echo Reply**.

3. **Validare și procesare header IP:**  
   - Se verifică integritatea header-ului IP folosind câmpul `checksum`.  
   - Dacă este invalid, pachetul este aruncat.  
   - Se scade **TTL-ul** cu 1; dacă TTL-ul devine 0, se trimite mesajul **ICMP Time Exceeded**.

4. **Determinarea rutei:**  
   - Se caută cea mai specifică rută în tabela de rutare (Longest Prefix Match).  
   - Dacă nu există o rută validă, se trimite mesaj **ICMP Destination Unreachable**.  
   - În caz contrar, se actualizează adresele și se transmite pachetul către următorul *hop*.

---

## 💬 Protocolul ICMP

În situațiile în care un pachet este aruncat de router, se generează un mesaj ICMP trimis înapoi către sursă.  
Pentru acest comportament am folosit funcția `icmp()`, modificând tipul mesajului în funcție de cauză (TTL expirat, destinație inexistentă etc.).

---

## ⚡ Longest Prefix Match (LPM) eficient

Pentru determinarea rutei optime am implementat o **căutare binară** în tabela de rutare, cu complexitate **O(log n)**.  
Pentru a putea aplica această metodă, tabela este **sortată** anterior folosind funcția `qsort()`.

---

## 🧩 Concluzii

Implementarea simulează comportamentul esențial al unui router IP la nivel de *dataplane*, incluzând:
- Tratarea pachetelor IPv4  
- Generarea mesajelor ICMP  
- Gestionarea TTL-ului  
- Determinarea eficientă a rutei prin LPM  


