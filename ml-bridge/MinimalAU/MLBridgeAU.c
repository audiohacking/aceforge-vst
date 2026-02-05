/*
 * ML Bridge AU — scaffold for connecting a DAW to your ML host.
 *
 * This file documents the minimal pieces; a full AU requires either:
 *   - Xcode: File → New → Project → Audio Unit Extension (Swift/Obj-C), or
 *   - JUCE: Audio Plug-In project with AU + VST3.
 *
 * Render callback pattern (use this in your AUAudioUnit subclass or JUCE processBlock):
 *   - In the render callback you get (timestamp, frameCount, outputBufferList).
 *   - Fill outputBufferList with float samples (stereo = 2 buffers, non-interleaved).
 *   - To use an ML host: connect to 127.0.0.1:8765 (or Unix socket), send
 *     (frameCount, sampleRate, optional MIDI), receive float32[] and copy into buffers.
 *
 * ACE Bridge 2 (from reverse engineering) uses socket/connect() and only system
 * frameworks; no custom SDK in the plugin. So your plugin can be a thin client.
 */

#include <AudioToolbox/AudioToolbox.h>
#include <CoreFoundation/CoreFoundation.h>

/* Export the factory name your Info.plist AudioComponents.factoryFunction must match.
 * Real implementation: use Xcode "Audio Unit Extension" template and implement
 * AUAudioUnit's internalRenderBlock to call your socket client or fill silence.
 */
__attribute__((visibility("default")))
void *MLBridgeAUFactory(void) {
    /* Placeholder: Xcode AU Extension template implements this and creates
     * an AUAudioUnit subclass that provides the render block. */
    return 0;
}
