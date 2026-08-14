# gm_8bit
A module for manipulating voice data in Garry's Mod.

# What does it do?
gm_8bit is designed to be a starting point for any kind of voice stream manipulation you might want to do on a Garry's Mod server (or any source engine server, with a bit of adjustment).

gm_8bit can decompress and recompress steam voice packets. It includes an SV_BroadcastVoiceData hook to allow server operators to incercept and manipulate this voice data. It makes several things possible, including:
* Relaying server voice data to external locations
* Performing voice recognition and producing transcripts
* Recording voice data in compressed or uncompressed form
* Applying transformation to user voice streams, for example pitch correction, noise suppression, or gain control.

gm_8bit currently has reference implementations for relaying voice data and applying transformations to voice streams. See the `voice-relay` repository for an example implementation of a server that uses gm_8bit to relay server voice communications to a discord channel.

# Builds
32-bit windows and linux builds are available with every commit. See the actions page.
This fork does not build 64-bit binaries.

# API
`eightbit.EnableBroadcast(bool)` Sets whether the module should relay voice packets to `localhost:4000`.

`eightbit.SetBroadcastIP(string)` Controls what IP the module should relay voice packets to, if broadcast is enabled.

`eightbit.SetBroadcastPort(number)` Controls what port the module should relay voice packets to, if broadcast is enabled. Must be 1-65535.

`eightbit.EnableEffects(userid, number)` Sets whether to enable audio effects for a given userid. Takes 1 (true) or 0 (false).

`eightbit.SetSampleRate(number)` Sets the sample rate written into the voice packet header, between 6000 and 48000. The encoder itself always runs at 24000 Hz, so this is what makes clients play the stream back faster or slower (a pitch shift), not a resample.

# Example
```lua
-- Called for every voice packet of a player that has effects enabled.
-- `samples` is a table of `count` signed 16-bit PCM values (-32768 to 32767) at 24000 Hz.
-- Modify it in place and return it; values are clamped back into int16 range.
-- Returning anything other than a table leaves the audio untouched.
hook.Add("ApplyVoiceEffect", "halve_volume", function(userId, samples, count)
    local ply = Player(userId)
    if not IsValid(ply) then return end

    for i = 1, count do
        samples[i] = samples[i] * 0.5
    end

    return samples
end)
```

**Performance note:** this hook runs on the main thread for every voice packet (roughly 20-40
per second per speaking player), and each call marshals up to ~10.000 samples into Lua and
back. Keep the callback cheap, and only enable effects for players that need them.
The `samples` table is reused between calls - do not hold on to it after the hook returns,
and always use `count` rather than `#samples`.

# Tests
`test/run.sh` builds and runs the stack-discipline tests for the voice hook against a mock
`ILuaBase`, under ASan/UBSan:

```
GMCOMMON=/path/to/garrysmod_common ./test/run.sh
```

