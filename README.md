# MixRef Strip v2.0

Plugin VST3 / AU para Mac (Apple Silicon e Intel) para mezclar con referencias de nivel, con el kick a -10 dBFS:

1. **Medidor de referencia.** Elegís qué es el canal entre los 33 elementos de la hoja de referencia y te marca en verde si el pico está en rango, en amarillo si está cerca y en rojo si se pasa. **Ajustar al objetivo** mueve la ganancia sola para dejar el pico en el valor ideal.
2. **EQ de 8 bandas**, con las funciones de un EQ Eight:
   - 8 tipos de filtro por banda: corte de graves 48 y 12 dB/oct, shelf de graves, campana, notch, shelf de agudos y corte de agudos 12 y 48 dB/oct.
   - Botones 1 a 8 para prender y apagar cada banda; gráfico con la curva en vivo, el analizador de espectro y puntos que se arrastran.
   - **Audición (auricular):** con el botón prendido, mantené apretado un punto o una perilla de la banda y escuchás solo la zona que toca. En los cortes y shelves escuchás justo lo que se saca.
   - **Delta:** escuchás solo lo que el EQ saca o agrega.
   - **Modos Estéreo, L/R y Mid/Side**, con 8 bandas propias para cada lado (botones L/R o Mid/Side para elegir cuál editás).
   - **Adaptive Q:** el Q de las campanas se cierra cuando subís mucha ganancia.
   - **Alta calidad (x2):** procesa el EQ al doble de frecuencia de muestreo, con agudos más naturales. Suma 49 muestras de latencia, que Ableton compensa solo.
   - **Escala** (de -200 % a 200 %, invierte la curva en negativo) y **Salida EQ**.
   - **Graves mono** debajo de la frecuencia que elijas.
   - **Cargar preset** pone el EQ de partida del elemento elegido.
3. **Ducker de sidechain.** Baja el volumen del canal cuando entra el kick por la entrada de sidechain.

Cadena de señal: ganancia → EQ → graves mono → ducker → medidor.

**Atajos del gráfico:** arrastrar un punto mueve frecuencia y ganancia (si la banda estaba apagada, se prende); la rueda del mouse sobre un punto cambia el Q; doble clic vuelve la ganancia a 0 dB.

**Sesiones de versiones anteriores:** al abrirlas, el EQ de 4 bandas se pasa solo a las bandas 1, 3, 5 y 7 del EQ nuevo, con los mismos valores.

---

## Cómo compilarlo (sin instalar nada en tu Mac)

GitHub lo compila gratis en una Mac en la nube.

1. Creá una cuenta en https://github.com si no tenés.
2. Arriba a la derecha: **+ → New repository**. Nombre: `MixRefStrip`. Puede ser público o privado. Tocá **Create repository**.
3. En la página del repositorio vacío, tocá **uploading an existing file** y arrastrá **el contenido** de esta carpeta: `CMakeLists.txt`, `README.md` y la carpeta `Source`. Tocá **Commit changes**.
4. La carpeta `.github` está oculta en el Finder, así que el workflow conviene crearlo a mano: **Add file → Create new file**, escribí como nombre `.github/workflows/build.yml` y pegá el contenido del archivo `build.yml` que viene en este zip (abrilo con TextEdit, o mostrá ocultos en el Finder con **Cmd + Shift + .**). Tocá **Commit changes**.
5. Andá a la pestaña **Actions**. Vas a ver "Compilar MixRef Strip (Mac)" corriendo (tarda unos 5 a 10 minutos). Cuando aparezca el tilde verde, entrá y abajo, en **Artifacts**, descargá **MixRefStrip-Mac**.

Cada vez que cambies algo del código y lo subas, se compila solo otra vez.

## Instalarlo

1. Descomprimí `MixRefStrip-Mac.zip`; adentro hay `MixRefStrip-VST3.zip` y `MixRefStrip-AU.zip`. Descomprimí los dos.
2. Copiá:
   - `MixRef Strip.vst3` → `~/Library/Audio/Plug-Ins/VST3/`
   - `MixRef Strip.component` → `~/Library/Audio/Plug-Ins/Components/`
   (En el Finder: menú **Ir → Ir a la carpeta…** y pegá la ruta.)
3. Como el plugin no está firmado por Apple, macOS lo bloquea. Abrí la **Terminal** y pegá:

```
xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/"MixRef Strip.vst3" ~/Library/Audio/Plug-Ins/Components/"MixRef Strip.component"
```

4. En Ableton: **Settings → Plug-ins**, activá *Use VST3 Plug-in System Folders* y *Use Audio Units*, y tocá **Rescan**. Aparece en **Plug-ins → Lucas Audio → MixRef Strip**.

## Uso rápido

- **Medidor:** ponelo al final de la cadena del canal (o solo, si ya tenés otros efectos). Elegí el elemento, dale play a la parte más fuerte del tema, tocá **Ajustar al objetivo** y listo. **Reset pico** borra el máximo para volver a medir.
- **Preset:** elegí el elemento y tocá **Cargar preset**. Es un punto de partida, después ajustá con el oído.
- **Ducker con el kick:** cargalo en el bajo (o synths/pads), activá **Ducker** y en el encabezado del dispositivo en Ableton desplegá la sección de sidechain y elegí la pista del kick como fuente. Cuando el indicador dice "Sidechain: recibiendo señal", subí la **Profundidad** (6 a 10 dB para el bajo, 3 a 5 para synths y pads) y ajustá el **Release** para que respire con el groove.
- **Graves mono:** suma a mono todo lo que esté debajo de la frecuencia elegida (120 Hz por defecto). Ideal para bajo y kick.
- **EQ con auricular:** prendé **Audición**, mantené apretado el punto de la banda y movelo buscando la frecuencia que molesta. Cuando la encontrás, soltás y bajás la ganancia.
- **Mid/Side en pads y synths:** elegí **Mid/Side**, tocá **Side** y poné un corte de graves en 150-250 Hz: los graves quedan en el centro y el estéreo se limpia.

## Valores por elemento

Rangos de la hoja **Niveles de mezcla** (progressive house / melodic). El objetivo es el punto medio de cada rango; el kick tiene 1 dB de margen alrededor de -10.

| Grupo | Elemento | Rango (pico dBFS) |
|---|---|---|
| Batería y percusión | Kick | -10 (verde de -11 a -9) |
| | Clap / Snare | -14 a -18 |
| | Percusión principal | -12 a -16 |
| | Percusión secundaria | -16 a -20 |
| | Closed hat | -16 a -20 |
| | Open hat | -15 a -19 |
| | Shakers | -18 a -22 |
| | Toms | -14 a -18 |
| | Rides | -18 a -22 |
| Bajos | Bass principal | -14 a -15 |
| | Sub bass | -15 a -18 |
| | Bass grupo / Layer | -16 a -20 |
| | Reese bass | -18 a -22 |
| | Mid bass | -16 a -20 |
| Elementos musicales | Lead / Synth principal | -15 a -19 |
| | Arp | -17 a -21 |
| | Pluck | -16 a -20 |
| | Chords / Acordes | -18 a -22 |
| | Piano / Keys | -18 a -22 |
| | Guitars | -18 a -22 |
| Fondo, atmósferas y vocales | Pads | -19 a -22 |
| | Strings | -18 a -23 |
| | Atmósferas | -22 a -28 |
| | Textures | -22 a -30 |
| | Vocal principal | -14 a -18 |
| | Vocal chops | -17 a -22 |
| FX, transiciones y detalles | Risers | -18 a -24 |
| | Impacts | -14 a -20 |
| | Downsweeps | -18 a -24 |
| | Noise / Sweeps | -20 a -26 |
| | Ear candy | -18 a -25 |
| | Reverb returns | -20 a -30 |
| | Delay returns | -18 a -26 |

Se cambian en `Source/PluginProcessor.cpp`, en `getElementPresets()`. Para agregar elementos, sumalos **al final** de esa lista (así las sesiones guardadas no cambian) y agregá su número en `getElementDisplayOrder()`.

Estos niveles son puntos de partida, no reglas fijas: el oído y el contexto son lo más importante.
