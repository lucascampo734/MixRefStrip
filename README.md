# MixRef Strip

Plugin VST3 / AU para Mac (Apple Silicon e Intel) que junta tres herramientas del checklist de mezcla, con el kick de referencia a -10 dBFS:

1. **Medidor de referencia.** Elegís qué es el canal (kick, bajo, clap, hats, percusión, synths, pads, vocal, toms) y te marca en verde si el pico está en el rango del checklist, en amarillo si está cerca y en rojo si se pasa. El botón **Ajustar al objetivo** mueve la ganancia sola para que el pico quede en el valor ideal.
2. **Channel strip.** Pasa altos de 24 dB/oct, corte de barro, presencia, aire y graves en mono. **Cargar preset** pone los valores de partida del elemento elegido.
3. **Ducker de sidechain.** Baja el volumen del canal cuando entra el kick por la entrada de sidechain.

4. **Gráfico del EQ (v1.1).** Curva de respuesta en vivo con el analizador de espectro de fondo. Los puntos se arrastran con el mouse: 1 pasa altos, 2 barro, 3 presencia, 4 aire y M el corte de graves mono. Doble clic vuelve la ganancia a 0 dB y la rueda del mouse sobre el punto 2 cambia el Q.

Cadena de señal: ganancia → EQ → graves mono → ducker → medidor.

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

## Valores por elemento

| Elemento | Pico objetivo | Rango verde |
|---|---|---|
| Kick | -10 | -11 a -9 |
| Bajo / Sub | -13 | -14 a -12 |
| Clap / Snare | -17 | -18 a -16 |
| Hats | -22 | -24 a -20 |
| Percusión | -20 | -22 a -18 |
| Synths / Arps | -18 | -20 a -16 |
| Pads / Atmos | -22 | -24 a -20 |
| Vocal / Chops | -15 | -17 a -13 |
| Toms | -17 | -19 a -15 |

Se cambian en `Source/PluginProcessor.cpp`, en la tabla `getElementPresets()`. Son orientativos: siempre escuchá y compará con tus referencias.
