#pragma once
// Magneto Transport - tape stocks (stage 6). docs/physics/09-stock.md.
//
// A stock is a COATING - its record process, the reference's tables generated per
// coating by reference/mtref --stock= - and a MEDIUM: what its particles, its
// coating's uniformity and its surface do (hiss, modulation noise, spacing,
// dropouts). Four stocks on three coatings: the shedding stock is the studio
// coating decayed. Every random property is a hash of the tape position, so a
// tape behaves the same every time it is played. No JUCE in here.

#include <cstdint>

namespace mt
{

constexpr int STK_NV = 5, STK_NL = 4, STK_NA = 64, STK_NS = 41, STK_NM = 11;
constexpr int STK_NHE = 12, STK_NT = 4;   // the DUB tables: erase-head fields, overdub levels
constexpr int NUM_COATINGS = 3;   // 0 studio (456 class), 1 vintage low-output, 2 budget
constexpr int NUM_STOCKS   = 4;   // STUDIO, VINTAGE, SHEDDING, BUDGET (the parameter's order)

// two-track 1/4-inch geometry, and the coating (held at 14 um for every stock)
constexpr double TRACK_W_UM   = 2000.0;   // track width
constexpr double TRACK_SEP_UM = 2750.0;   // centre to centre
constexpr double COATING_UM   = 14.0;

/*  One coating's record tables and its alignment, as generated. */
struct CoatingTables
{
    const char* name;
    double fluxMrNwbm;                       // flux at Mr over the coating
    double hbOperating, hs0vu, alignSpeedCms;
    const double* speedCms;                  // [NV]
    const double* layerFmax;                 // [NL]
    const double* aGrid;                     // [NA]
    const double* sGrid;                     // [NS]
    const double (*mRem)[STK_NM];            // [NL][NM]: each slab's old-state grid
    const double (*zoneX)[STK_NL][STK_NA];   // [NV][NL][NA]
    const double (*zoneW)[STK_NL][STK_NA];
    const double (*zoneGain)[STK_NL][STK_NA];
    const float (*G)[STK_NL][STK_NA][STK_NM][STK_NS];   // [NV][NL][NA][NM][NS]
    // what one pass of the loop's heads leaves (mtref --dubtables, 10-flux-memory.md): the
    // erase head at HE_GRID[h] (deep gap, Hc) then the bias at A_GRID[a], no signal, as a
    // RATIO of the remanence: of the committed take (recorded at the operating bias, on
    // the slab's M_REM grid) and of an overdub (a DC level TOP_LEVEL x 0 VU recorded at
    // that same bias; TOP_M is the remanence it holds)
    const double* heGrid;                    // [NHE]
    const double* topLevel;                  // [NT]
    const float (*rDub)[STK_NL][STK_NHE][STK_NA][STK_NM];
    const float (*topM)[STK_NL][STK_NA][STK_NT];
    const float (*rTop)[STK_NL][STK_NHE][STK_NA][STK_NT];
    const double* pbSpeedCms;                // [NV]
    const double* pbSpacingUm;               // [NV]: the slabs' effective extra spacing (stage 4)
    double alignSpacingUm;                   // the fresh stock's surface spacing the machine is aligned to
};
const CoatingTables& coatingTables (int coating);

/*  What a stock's particles, coating and surface do. */
struct StockMedium
{
    const char* name;
    int    coating;
    double packing;          // remanence relative to the coating's own (a decayed tape has lost particles)
    double opLevelNwbm;      // the operating level the machine is aligned to for it (0 VU)
    double particleVolUm3;   // particle volume / packing fraction (um^3): sets the hiss
    double modNoise;         // rms coating density variation: modulation noise
    double roughUm;          // rms surface roughness: spacing fluctuation (um)
    double spacingUm;        // the surface's average extra spacing (um)
    double dropPerM;         // dropouts per metre of tape
    double dropDepthUm;      // their mean extra spacing (um)
    double holeFrac;         // the share that are holes in the coating (shed oxide), not lift
    double wearPerPass;      // ACCUMULATE: wear added by one pass of a loop over the heads
    double friction;         // stick-slip, relative to the studio stock (a sticky binder squeals)
};
const StockMedium& stockMedium (int stock);

//------------------------------------------------------------------------------ hashes
// splitmix64: a good 64-bit mix, so a tape position is all the seed a property needs
inline uint64_t mix64 (uint64_t z)
{
    z += 0x9E3779B97F4A7C15ull;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}
inline uint64_t key3 (int64_t a, uint64_t b, uint64_t c) { return mix64 ((uint64_t) a ^ mix64 (b * 0x632BE59BD9B4E019ull + c)); }
inline double hashUni (uint64_t k) { return (double) (k >> 11) * (1.0 / 9007199254740992.0); }       // [0, 1)
inline double hashSym (uint64_t k) { return (hashUni (k) - 0.5) * 3.4641016151377544; }              // uniform, unit variance

/*  Smooth random function of a position: Catmull-Rom through unit-variance lattice
    values `lattice` apart. Correlated over about one lattice step. */
double valueNoise (double x, double lattice, uint64_t seed);

} // namespace mt
