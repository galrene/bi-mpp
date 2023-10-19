# BI-MPP-PIC24-USB-Simulator

Tento projekt obsahuje simulátor usb subsystému mikrokontroléru pic24fj256gb106, 
který je využíván v předmětu BI-MPP. Změnou souboru main.c lze vytvořit USB zařízení 
se stejným zdrojovým kódem, jako by se jednalo o hardwarové
přípravky MPLAB PIC24 a vývojové prostředí MPLAB. 
Simulátor adoptoval knihoval usb_lib, která je v předmětu BI-MPP používána. 

Simulátor má USBIP backend, tak je možné se pomocí USBIP příkazů 
připojit k simulátoru a usb zařízení zařadit mezi ostatní USB zařízení v systému.
Příkaz lsusb připojené zařízení zobrazí. Se zařízením pak můžete komunikovat  
přes standardní knihovnu libusb-1.0.

> **IMPORTANT:**
> Při spuštění na Linuxu na Virtual Boxu nastavte minimálně 3 lépe 4 jádra. Jinak simulátor nemusí fungovat.

## Vlastnosti

* USB reset (**podporováno**)
* USB control transfers (**podporováno**)
* USB bulk transfers (**podporováno**)
* USB interrupt transfers (**není podporováno**)
* USB ISO transfers (**není podporováno**)
* Pouze jedno zařízení
* SOF (**není podporováno**)
* Frontend usb_lib (viz. BI-MPP)
* Backend USBIP server
* Založeno na OpenHCI specifikaci

## Prerekvizity

```bash
apt-get install usbip (na různých distribucích se může lišit)
```

Jména balíčků na různých systémech:
* usbip (Debian)
* linux-oem-5.6-tools-common
* selinux-tools ??

## Připojení k simulátoru

Spustit simulátor ./main a pak

```bash
 modprobe vhci_hcd
 usbip list -r 127.0.0.1
 usbip attach -r 127.0.0.1 -b 1-1
```

Poznámka: není nutné, aby simulátor běžel na stejném systému kam pomocí usbip mapujeme 
vyvíjené zařízení. Může být na jiném počítači nebo ve virtuálu. Podmínkou je síťové připojení.


## Překlad simulátoru (build)

```bash
   make clean
   make
```    
Překlad proběhne včetně překladu souboru main.c s kódem zařízení. Simulátor pak spouštíme příkazem ./main. 

Jako side efekt překladu se vytvoří knihovna 
libpic24usbsim.a, kterou lze přilinkovat ke zdrojovému kódu zařízení. Nechceme-li modifikovat main.c, pak 
alternativní sestavení je

```bash
    gcc -o device device.c -L . -l pic24usbsim -l pthread
```
a spouštíme ./device 


