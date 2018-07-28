/**
 * @file
 * Simulation Manager
 *
 * All REvoSim code is released under the GNU General Public License.
 * See LICENSE.md files in the programme directory.
 *
 * All REvoSim code is Copyright 2008-2018 by Mark D. Sutton, Russell J. Garwood,
 * and Alan R.T. Spencer.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at
 * your option) any later version. This program is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY.
 */

#include "simmanager.h"

//RJG - so can access mainWindow
#include "mainwindow.h"

#include <QDebug>
#include <cstdlib>
#include <cmath>
#include <QThread>
#include <QImage>
#include <QMessageBox>

//Simulation variables
quint32 tweakers[32]; // the 32 single bit XOR values (many uses!)
quint64 tweakers64[64]; // the 64 bit version
quint32 bitcounts[65536]; // the bytes representing bit count of each number 0-635535
quint32 xormasks[256][3]; //determine fitness
int xdisp[256][256];
int ydisp[256][256];
quint64 genex[65536];
int nextgenex;

quint64 reseedGenome = 0; //RJG - Genome for reseed with known genome

//Settable ints
int gridX = 100;        //Can't be used to define arrays - hence both ATM
int gridY = 100;
int slotsPerSquare = 100;
int startAge = 15;
int target = 66;
int settleTolerance = 15;
int dispersal = 15;
int food = 3000;
int breedThreshold = 500;
int breedCost = 500;
int maxDiff = 2;
int mutate = 10;
int environmentChangeRate = 100;
int speciesSamples = 1;
int speciesSensitivity = 2;
int timeSliceConnect = 5;
quint64 lastReport = 0;

//Settable bools
bool recalculateFitness = false;
bool asexual = false;
bool sexual = true;
bool logging = false;
bool fitnessLoggingToFile = false;
bool nonspatial = false;
bool environmentInterpolate = true;
bool toroidal = false;
bool reseedKnown = false;
bool breedspecies = false;
bool breeddiff = true;
bool gui = false;

//File handling
QStringList environmentFiles;
int currentEnvironmentFile;
int EnvChangeCounter;
bool EnvChangeForward;
QString speciesLoggingFile = "";
QString FitnessLoggingFile = "";

//Globabl data
Critter critters[GRID_X][GRID_Y][SLOTS_PER_GRID_SQUARE]; //main array - static for speed
quint8 environment[GRID_X][GRID_Y][3];  //0 = red, 1 = green, 2 = blue
quint8 environmentlast[GRID_X][GRID_Y][3];  //Used for interpolation
quint8 environmentnext[GRID_X][GRID_Y][3];  //Used for interpolation
quint32 totalfit[GRID_X][GRID_Y];
quint64 generation;

//These next to hold the babies... old style arrays for max speed
quint64 newgenomes[GRID_X * GRID_Y * SLOTS_PER_GRID_SQUARE * 2];
quint32 newgenomeX[GRID_X * GRID_Y * SLOTS_PER_GRID_SQUARE * 2];
quint32 newgenomeY[GRID_X * GRID_Y * SLOTS_PER_GRID_SQUARE * 2];
int newgenomeDisp[GRID_X * GRID_Y * SLOTS_PER_GRID_SQUARE * 2];
quint64 newgenomespecies[GRID_X * GRID_Y * SLOTS_PER_GRID_SQUARE * 2];
int newgenomecount;
quint8 randoms[65536];
quint16 nextrandom = 0;

//Analysis
int breedattempts[GRID_X][GRID_Y]; //for analysis purposes
int breedfails[GRID_X][GRID_Y]; //for analysis purposes
int settles[GRID_X][GRID_Y]; //for analysis purposes
int settlefails[GRID_X][GRID_Y]; //for analysis purposes
int maxused[GRID_X][GRID_Y];
int aliveCount;
int totalRecombination;

//Species stuff
QList<Species> oldSpeciesList;
QList< QList<Species> > archivedspecieslists; //no longer used?
LogSpecies *rootspecies;
QHash<quint64, LogSpecies *> LogSpeciesById;
quint64 lastSpeciesCalc = 0;
quint64 nextspeciesid;
QList<uint> species_colours;
quint8 speciesMode;
quint64 ids; //used in tree export -

// Environment Stuff
quint8 environmentMode;

quint64 minSpeciesSize;
bool allowExcludeWithDescendants;

QMutex *mutexes[GRID_X][GRID_Y]; //set up array of mutexes


SimManager::SimManager()
{
    //Constructor - set up all the data!
    speciesMode = SPECIES_MODE_BASIC;
    environmentMode = ENV_MODE_LOOP;
    environmentInterpolate = true;
    MakeLookups();
    aliveCount = 0;
    ProcessorCount = QThread::idealThreadCount();
    if (ProcessorCount == -1) ProcessorCount = 1;
    if (ProcessorCount > 256) ProcessorCount = 256; //a sanity check
    //ProcessorCount=1;
    for (auto &mutexe : mutexes)
        for (int j = 0; j < GRID_X; j++)
            mutexe[j] = new QMutex();

    for (int i = 0; i < ProcessorCount; i++)
        FuturesList.append(new QFuture<int>);


    environmentFiles.clear();
    currentEnvironmentFile = -1;
    EnvChangeCounter = 0;
    EnvChangeForward = true;
    nextspeciesid = 1;
    rootspecies = (LogSpecies *)nullptr;

    warning_count = 0;
}


int SimManager::portable_rand()
{
    //replacement for qrand to come with RAND_MAX !=32767

    if (RAND_MAX < 32767) {
        qDebug() << "RAND_MAX too low - it's " << RAND_MAX;
        exit(0);
    }
    if (RAND_MAX > 32767) {
        // assume it's (2^n)-1
        int r = qrand();
        return r & 32767; //mask off bottom 16 bits, return those
    }
    return qrand();
}

void SimManager::MakeLookups()
{
    //These are 00000001, 000000010, 0000000100 etc
    tweakers[0] = 1;
    for (int n = 1; n < 32; n++) tweakers[n] = tweakers[n - 1] * 2;

    tweakers64[0] = 1;
    for (int n = 1; n < 64; n++) tweakers64[n] = tweakers64[n - 1] * 2;

    //and now the bitcounting...
    // set up lookup 0 to 65535 to enable bits to be counted for each
    for (qint32 n = 0; n < 65536; n++) {
        qint32 count = 0;
        for (int m = 0; m < 16; m++) if ((n & tweakers[m]) != 0) ++count; // count the bits
        bitcounts[n] = count;
    }

    //RJG - seed random from time qsrand(RAND_SEED);
    qsrand(QTime::currentTime().msec());

    //now set up xor masks for 3 variables - these are used for each of R G and B to work out fitness
    //Start - random bit pattern for each
    xormasks[0][0] = portable_rand() * portable_rand() * 2;
    xormasks[0][1] = portable_rand() * portable_rand() * 2;
    xormasks[0][2] = portable_rand() * portable_rand() * 2;

    for (int n = 1; n < 256;
            n++) { //for all the others - flip a random bit each time (^ is xor) - will slowly modify from 0 to 255
        xormasks[n][0] = xormasks[n - 1][0] ^ tweakers[portable_rand() / (PORTABLE_RAND_MAX / 32)];
        xormasks[n][1] = xormasks[n - 1][1] ^ tweakers[portable_rand() / (PORTABLE_RAND_MAX / 32)];
        xormasks[n][2] = xormasks[n - 1][2] ^ tweakers[portable_rand() / (PORTABLE_RAND_MAX / 32)];
    }

    //now the randoms - pre_rolled random numbers 0-255
    for (unsigned char &random : randoms) random = (quint8)((portable_rand() & 255));
    nextrandom = 0;

    // gene exchange lookup
    for (unsigned long long &n : genex) {  //random bit combs, averaging every other bit on
        quint64 value = 0;
        for (unsigned long long m : tweakers64) if (portable_rand() > (PORTABLE_RAND_MAX / 2)) value += m;
        n = value;
    }
    nextgenex = 0;

    //dispersal table - lookups for dispersal amount
    //n is the distance to be dispersed - biased locally (the sqrt)
    //m is angle
    for (int n = 0; n < 256; n++) {
        double d = sqrt(65536 / (double)(n + 1)) - 16;
        if (d < 0) d = 0;
        for (int m = 0; m < 256; m++) {
            xdisp[n][m] = (int)(d * sin((double)(m) / 40.5845));
            ydisp[n][m] = (int)(d * cos((double)(m) / 40.5845));
        }
    }

    //colours
    for (int i = 0; i < 65536; i++) {
        species_colours.append(qRgb(Rand8(), Rand8(), Rand8()));
    }
}


void SimManager::loadEnvironmentFromFile(int emode)
// Load current envirnonment from file
{
    //Use make qimage from file method
    //Load the image
    if (currentEnvironmentFile >= environmentFiles.count()) {
        return;
    }
    QImage LoadImage(environmentFiles[currentEnvironmentFile]);

    if (LoadImage.isNull()) {
        QMessageBox::critical(nullptr, "Error", "Fatal - can't open image " + environmentFiles[currentEnvironmentFile]);
        exit(1);
    }
    //check size works
    int xsize = LoadImage.width();
    int ysize = LoadImage.height();

    if (xsize < gridX || ysize < gridY) //rescale if necessary - only if too small
        LoadImage = LoadImage.scaled(QSize(gridX, gridY), Qt::IgnoreAspectRatio);

    //turn into environment array
    for (int i = 0; i < gridX; i++)
        for (int j = 0; j < gridY; j++) {
            QRgb colour = LoadImage.pixel(i, j);
            environment[i][j][0] = qRed(colour);
            environment[i][j][1] = qGreen(colour);
            environment[i][j][2] = qBlue(colour);
        }

    //set up environmentlast - same as environment
    for (int i = 0; i < gridX; i++)
        for (int j = 0; j < gridY; j++) {
            QRgb colour = LoadImage.pixel(i, j);
            environmentlast[i][j][0] = qRed(colour);
            environmentlast[i][j][1] = qGreen(colour);
            environmentlast[i][j][2] = qBlue(colour);
        }
    //set up environment next - depends on emode

    if (emode == 0 || environmentFiles.count() == 1) { //static environment
        for (int i = 0; i < gridX; i++)
            for (int j = 0; j < gridY; j++) {
                QRgb colour = LoadImage.pixel(i, j);
                environmentnext[i][j][0] = qRed(colour);
                environmentnext[i][j][1] = qGreen(colour);
                environmentnext[i][j][2] = qBlue(colour);
            }
    } else {
        //work out next file
        int nextfile;
        if (EnvChangeForward) {
            if ((currentEnvironmentFile + 1) < environmentFiles.count()) //not yet at end
                nextfile = currentEnvironmentFile + 1;
            else {
                //depends on emode
                if (emode == 1) nextfile = currentEnvironmentFile; //won't matter
                if (emode == 2) nextfile = 0; //loop mode
                if (emode == 3) nextfile = currentEnvironmentFile - 1; //bounce mode
            }
        } else { //backwards - simpler, must be emode 3
            if (currentEnvironmentFile > 0) //not yet at end
                nextfile = currentEnvironmentFile - 1;
            else
                nextfile = 1; //bounce mode
        }

        QImage LoadImage2(environmentFiles[nextfile]);
        if (xsize < gridX || ysize < gridY) //rescale if necessary - only if too small
            LoadImage2 = LoadImage2.scaled(QSize(gridX, gridY), Qt::IgnoreAspectRatio);
        //get it
        for (int i = 0; i < gridX; i++)
            for (int j = 0; j < gridY; j++) {
                QRgb colour = LoadImage2.pixel(i, j);
                environmentnext[i][j][0] = qRed(colour);
                environmentnext[i][j][1] = qGreen(colour);
                environmentnext[i][j][2] = qBlue(colour);
            }
    }
}

bool SimManager::regenerateEnvironment(int emode, bool interpolate)
//returns true if finished sim
{
    if (environmentChangeRate == 0 || emode == 0
            || environmentFiles.count() == 1) return
                    false; //constant environment - either static in menu, or 0 environmentChangeRate, or only one file

    --EnvChangeCounter;

    if (EnvChangeCounter <= 0)
        //is it time to do a full change?
    {
        if (emode != 3 && !EnvChangeForward) //should not be going backwards!
            EnvChangeForward = true;
        if (EnvChangeForward) {
            currentEnvironmentFile++; //next image
            if (currentEnvironmentFile >= environmentFiles.count()) {
                if (emode == 1) return true; //no more files and we are in 'once' mode - stop the sim
                if (emode == 2) currentEnvironmentFile = 0; //loop mode
                if (emode == 3) {
                    currentEnvironmentFile -= 2; //bounce mode - back two to undo the extra ++
                    EnvChangeForward = false;
                }
            }
        } else { //going backwards - must be in emode 3 (bounce)
            currentEnvironmentFile--; //next image
            if (currentEnvironmentFile < 0) {
                currentEnvironmentFile = 1; //bounce mode - one to one again, must have just been 0
                EnvChangeForward = true;
            }
        }
        EnvChangeCounter = environmentChangeRate; //reset counter
        loadEnvironmentFromFile(emode); //and load it from the file

    } else {
        if (interpolate) {
            float progress, invprogress;
            invprogress = ((float)(EnvChangeCounter + 1) / ((float)environmentChangeRate));
            progress = 1 - invprogress;
            //not getting new, doing an interpolate
            for (int i = 0; i < gridX; i++)
                for (int j = 0; j < gridY; j++) {
                    environment[i][j][0] = qint8(0.5 + ((float)environmentlast[i][j][0]) * invprogress + ((
                                                                                                              float)environmentnext[i][j][0]) * progress);
                    environment[i][j][1] = qint8(0.5 + ((float)environmentlast[i][j][1]) * invprogress + ((
                                                                                                              float)environmentnext[i][j][1]) * progress);
                    environment[i][j][2] = qint8(0.5 + ((float)environmentlast[i][j][2]) * invprogress + ((
                                                                                                              float)environmentnext[i][j][2]) * progress);
                }
        }

    }
    return false;
}

//----RJG: 64 bit rand useful for initialising critters
quint64 SimManager::Rand64()
{
    return (quint64)Rand32() + (quint64)(65536) * (quint64)(65536) * (quint64)Rand32();
}

quint32 SimManager::Rand32()
{
    //4 lots of RAND8
    quint32 rand1 = portable_rand() & 255;
    quint32 rand2 = (portable_rand() & 255) * 256;
    quint32 rand3 = (portable_rand() & 255) * 256 * 256;
    quint32 rand4 = (portable_rand() & 255) * 256 * 256 * 256;

    return rand1 + rand2 + rand3 + rand4;
}

quint8 SimManager::Rand8()
{
    return randoms[nextrandom++];
}

void SimManager::setupRun()
{

    //Find middle square, try creatures till something lives, duplicate it [slots] times
    //RJG - called on initial program load and reseed, but also when run/run for are hit
    //RJG - with modification for dual seed if selected

    //Kill em all
    for (int n = 0; n < gridX; n++)
        for (int m = 0; m < gridY; m++) {
            for (int c = 0; c < slotsPerSquare; c++) {
                critters[n][m][c].age = 0;
                critters[n][m][c].fitness = 0;
            }
            totalfit[n][m] = 0;
            maxused[n][m] = -1;
            breedattempts[n][m] = 0;
            breedfails[n][m] = 0;
            settles[n][m] = 0;
            settlefails[n][m] = 0;
        }

    nextspeciesid = 1; //reset ID counter

    int n = gridX / 2, m = gridY / 2;

    //RJG - Either reseed with known genome if set
    if (reseedKnown) {
        critters[n][m][0].initialise(reseedGenome, environment[n][m], n, m, 0, nextspeciesid);
        if (critters[n][m][0].fitness == 0) {
            // RJG - But sort out if it can't survive...
            QMessageBox::warning(nullptr, "Oops",
                                 "The genome you're trying to reseed with can't survive in this environment. There could be a number of reasons why this is. Please contact RJG or MDS to discuss.");
            reseedKnown = false;
            setupRun();
            return;
        }

        //RJG - I think this is a good thing to flag in an obvious fashion.
        QString reseedGenomeString("Started simulation with known genome: ");
        for (unsigned long long i : tweakers64)if (i & reseedGenome) reseedGenomeString.append("1");
            else reseedGenomeString.append("0");
        mainWindow->setStatusBarText(reseedGenomeString);
    } else {
        while (critters[n][m][0].fitness < 1) critters[n][m][0].initialise(Rand64(), environment[n][m], n,
                                                                               m, 0, nextspeciesid);
        mainWindow->setStatusBarText("");
    }

    totalfit[n][m] = critters[n][m][0].fitness; //may have gone wrong from above

    aliveCount = 1;
    quint64 generation = critters[n][m][0].genome;

    //RJG - Fill square with successful critter
    for (int c = 1; c < slotsPerSquare; c++) {
        critters[n][m][c].initialise(generation, environment[n][m], n, m, c, nextspeciesid);

        if (critters[n][m][c].age > 0) {
            critters[n][m][c].age /= ((Rand8() / 10) + 1);
            critters[n][m][c].age += 10;
            aliveCount++;
            maxused[n][m] = c;
            totalfit[n][m] += critters[n][m][c].fitness;
        }
    }

    generation = 0;

    EnvChangeCounter = environmentChangeRate;
    EnvChangeForward = true;

    //remove old species log if one exists
    delete rootspecies;

    //create a new logspecies with appropriate first data entry
    rootspecies = new LogSpecies;

    rootspecies->maxSize = aliveCount;
    rootspecies->ID = nextspeciesid;
    rootspecies->timeOfFirstAppearance = 0;
    rootspecies->timeOfLastAppearance = 0;
    rootspecies->parent = (LogSpecies *)nullptr;
    auto *newdata = new LogSpeciesDataItem;
    newdata->centroidRangeX = n;
    newdata->centroidRangeY = m;
    newdata->generation = 0;
    newdata->cellsOccupied = 1;
    newdata->genomicDiversity = 1;
    newdata->size = aliveCount;
    newdata->geographicalRange = 0;
    newdata->cellsOccupied = 0; //=1, this is stored as -1
    newdata->sampleGenome = generation;
    newdata->maxEnvironment[0] = environment[n][m][0];
    newdata->maxEnvironment[1] = environment[n][m][1];
    newdata->maxEnvironment[2] = environment[n][m][2];
    newdata->minEnvironment[0] = environment[n][m][0];
    newdata->minEnvironment[1] = environment[n][m][1];
    newdata->minEnvironment[2] = environment[n][m][2];
    newdata->meanEnvironment[0] = environment[n][m][0];
    newdata->meanEnvironment[1] = environment[n][m][1];
    newdata->meanEnvironment[2] = environment[n][m][2];
    newdata->meanFitness = (quint16)((totalfit[n][m] * 1000) / aliveCount);

    rootspecies->dataItems.append(newdata);
    LogSpeciesById.clear();
    LogSpeciesById.insert(nextspeciesid, rootspecies);

    //RJG - Depreciated, but clear here just in case
    archivedspecieslists.clear();

    oldSpeciesList.clear();
    Species newsp;
    newsp.ID = nextspeciesid;
    newsp.originTime = 0;
    newsp.parent = 0;
    newsp.size = slotsPerSquare;
    newsp.type = generation;
    newsp.logSpeciesStructure = rootspecies;
    oldSpeciesList.append(newsp);

    nextspeciesid++; //ready for first species after this

    //RJG - reset warning system
    warning_count = 0;
}

int SimManager::iterateParallel(int firstx, int lastx, int newGenomeCountLocal,
                                int *killCountLocal)
//parallel version - takes newgenomes_local as the start point it can write to in main genomes array
//returns number of new genomes
{
    int breedlist[SLOTS_PER_GRID_SQUARE];
    int maxalive;
    int deathcount;

    for (int n = firstx; n <= lastx; n++)
        for (int m = 0; m < gridY; m++) {
            int maxv = maxused[n][m];

            Critter *crit = critters[n][m];

            if (recalculateFitness) {
                totalfit[n][m] = 0;
                maxalive = 0;
                deathcount = 0;
                for (int c = 0; c <= maxv; c++) {
                    if (crit[c].age) {
                        quint32 f = crit[c].recalculateFitness(environment[n][m]);
                        totalfit[n][m] += f;
                        if (f > 0) maxalive = c;
                        else deathcount++;
                    }
                }
                maxused[n][m] = maxalive;
                maxv = maxalive;
                (*killCountLocal) += deathcount;
            }

            // RJG - reset counters for fitness logging to file
            if (fitnessLoggingToFile || logging)breedattempts[n][m] = 0;

            if (totalfit[n][m]) { //skip whole square if needbe
                int addFood = 1 + (food / totalfit[n][m]);

                int breedlistentries = 0;

                for (int c = 0; c <= maxv; c++)
                    if (crit[c].iterateParallel(killCountLocal, addFood)) breedlist[breedlistentries++] = c;

                // ----RJG: breedattempts was no longer used - co-opting for fitness report.
                if (fitnessLoggingToFile || logging)breedattempts[n][m] = breedlistentries;

                //----RJG Do breeding
                if (breedlistentries > 0) {
                    quint8 divider = 255 /
                                     breedlistentries; //originally had breedlistentries+5, no idea why. //lol - RG
                    for (int c = 0; c < breedlistentries; c++) {
                        int partner;
                        bool temp_asexual = asexual;

                        if (temp_asexual)partner = c;
                        else partner = Rand8() / divider;

                        if (partner < breedlistentries) {
                            if (crit[breedlist[c]].breedWithParallel(n, m, &(crit[breedlist[partner]]),
                                                                     &newGenomeCountLocal))
                                breedfails[n][m]++; //for analysis purposes
                        } else //didn't find a partner, refund breed cost
                            crit[breedlist[c]].energy += breedCost;
                    }
                }

            }
        }

    return newGenomeCountLocal;
}

int SimManager::settle_parallel(int newgenomecounts_start, int newgenomecounts_end,
                                int *trycount_local, int *settlecount_local, int *birthcounts_local)
{
    if (nonspatial) {
        //settling with no geography - just randomly pick a cell
        for (int n = newgenomecounts_start; n < newgenomecounts_end; n++) {
            quint64 xPosition = ((quint64)Rand32()) * (quint64)gridX;
            xPosition /= (((quint64)65536) * ((quint64)65536));
            quint64 yPosition = ((quint64)Rand32()) * (quint64)gridY;
            yPosition /= (((quint64)65536) * ((quint64)65536));

            mutexes[(int)xPosition][(int)yPosition]->lock(); //ensure no-one else buggers with this square
            (*trycount_local)++;
            Critter *crit = critters[(int)xPosition][(int)yPosition];
            //Now put the baby into any free slot here
            for (int m = 0; m < slotsPerSquare; m++) {
                Critter *crit2 = &(crit[m]);
                if (crit2->age == 0) {
                    //place it

                    crit2->initialise(newgenomes[n], environment[xPosition][yPosition], xPosition, yPosition, m, newgenomespecies[n]);
                    if (crit2->age) {
                        int fit = crit2->fitness;
                        totalfit[xPosition][yPosition] += fit;
                        (*birthcounts_local)++;
                        if (m > maxused[xPosition][yPosition]) maxused[xPosition][yPosition] = m;
                        settles[xPosition][yPosition]++;
                        (*settlecount_local)++;
                    } else settlefails[xPosition][yPosition]++;
                    break;
                }
            }
            mutexes[xPosition][yPosition]->unlock();
        }
    } else {
        //old code - normal settling with radiation from original point
        for (int n = newgenomecounts_start; n < newgenomecounts_end; n++) {
            //first handle dispersal

            quint8 t1 = Rand8();
            quint8 t2 = Rand8();

            int xPosition = (xdisp[t1][t2]) / newgenomeDisp[n];
            int yPosition = (ydisp[t1][t2]) / newgenomeDisp[n];
            xPosition += newgenomeX[n];
            yPosition += newgenomeY[n];


            if (toroidal) {
                //NOTE - this assumes max possible settle distance is less than grid size. Otherwise it will go tits up
                if (xPosition < 0) xPosition += gridX;
                if (xPosition >= gridX) xPosition -= gridX;
                if (yPosition < 0) yPosition += gridY;
                if (yPosition >= gridY) yPosition -= gridY;
            } else {
                if (xPosition < 0) continue;
                if (xPosition >= gridX)  continue;
                if (yPosition < 0)  continue;
                if (yPosition >= gridY)  continue;
            }

            mutexes[xPosition][yPosition]->lock(); //ensure no-one else buggers with this square
            (*trycount_local)++;
            Critter *crit = critters[xPosition][yPosition];
            //Now put the baby into any free slot here
            for (int m = 0; m < slotsPerSquare; m++) {
                Critter *crit2 = &(crit[m]);
                if (crit2->age == 0) {
                    //place it

                    crit2->initialise(newgenomes[n], environment[xPosition][yPosition], xPosition, yPosition, m, newgenomespecies[n]);
                    if (crit2->age) {
                        int fit = crit2->fitness;
                        totalfit[xPosition][yPosition] += fit;
                        (*birthcounts_local)++;
                        if (m > maxused[xPosition][yPosition]) maxused[xPosition][yPosition] = m;
                        settles[xPosition][yPosition]++;
                        (*settlecount_local)++;
                    } else settlefails[xPosition][yPosition]++;
                    break;
                }
            }
            mutexes[xPosition][yPosition]->unlock();

        }
    }
    return 0;
}


bool SimManager::iterate(int emode, bool interpolate)
{
    generation++;

    //RJG - Provide user with warning if the system is grinding through so many species it's taking>5 seconds.Option to turn off species mode.
    if (warning_count == 1) {
        if (QMessageBox::question(nullptr, "A choice awaits...",
                                  "The last species search took more than five seconds."
                                  " This suggests the settings you are using lend themselves towards speciation, and the species system is a bottleneck."
                                  " Would you like to switch off the species system? If you select no, a progress bar will appear to give you an idea of how long it is taking."
                                  "If you click yes, the system will be disabled. You will only see this warning once per run.",
                                  QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes) == QMessageBox::Yes) {
            speciesMode = 0;
            mainWindow->updateGUIFromVariables();
        }
        warning_count++;
    }

    if (regenerateEnvironment(emode, interpolate)) return true;

    //New parallelised version

    int newgenomecounts_starts[256]; //allow for up to 256 threads
    int newgenomecounts_ends[256]; //allow for up to 256 threads

    //work out positions in genome array that each thread can write to to guarantee no overlap
    int positionadd = (GRID_X * GRID_Y * SLOTS_PER_GRID_SQUARE * 2) / ProcessorCount;
    for (int i = 0; i < ProcessorCount; i++)
        newgenomecounts_starts[i] = i * positionadd;

    int KillCounts[256];
    for (int i = 0; i < ProcessorCount; i++) KillCounts[i] = 0;

    //do the magic! Set up futures objects, call the functions, wait till done, retrieve values

    for (int i = 0; i < ProcessorCount; i++)
        *(FuturesList[i]) = QtConcurrent::run(this, &SimManager::iterateParallel,
                                              (i * gridX) / ProcessorCount, (((i + 1) * gridX) / ProcessorCount) - 1, newgenomecounts_starts[i],
                                              &(KillCounts[i]));

    for (int i = 0; i < ProcessorCount; i++)
        FuturesList[i]->waitForFinished();

    for (int i = 0; i < ProcessorCount; i++)
        newgenomecounts_ends[i] = FuturesList[i]->result();

    //Testbed - call parallel functions, but in series
    /*
      for (int i=0; i<ProcessorCount; i++)
            newgenomecounts_ends[i]=SimManager::iterateParallel((i*gridX)/ProcessorCount, (((i+1)*gridX)/ProcessorCount)-1,newgenomecounts_starts[i], &(KillCounts[i]));
    */

    //apply all the kills to the global count
    for (int i = 0; i < ProcessorCount; i++)
        aliveCount -= KillCounts[i];

    //Now handle spat settling

    int trycount = 0;
    int settlecount = 0;

    int trycounts[256];
    for (int i = 0; i < ProcessorCount; i++) trycounts[i] = 0;
    int settlecounts[256];
    for (int i = 0; i < ProcessorCount; i++) settlecounts[i] = 0;
    int birthcounts[256];
    for (int i = 0; i < ProcessorCount; i++) birthcounts[i] = 0;

    //call the parallel settling function - in series for now
    /*    for (int i=0; i<ProcessorCount; i++)
            settle_parallel(newgenomecounts_starts[i],newgenomecounts_ends[i],&(trycounts[i]), &(settlecounts[i]), &(birthcounts[i]));
    */

    //Parallel version of settle functions
    for (int i = 0; i < ProcessorCount; i++)
        *(FuturesList[i]) = QtConcurrent::run(this, &SimManager::settle_parallel, newgenomecounts_starts[i],
                                              newgenomecounts_ends[i], &(trycounts[i]), &(settlecounts[i]), &(birthcounts[i]));

    for (int i = 0; i < ProcessorCount; i++)
        FuturesList[i]->waitForFinished();

    //sort out all the counts
    for (int i = 0; i < ProcessorCount; i++) {
        aliveCount += birthcounts[i];
        trycount += trycounts[i];
        settlecount += settlecounts[i];
    }

    return false;
}

void SimManager::testcode()//Use for any test with debugger, triggers from menu item
{
    qDebug() << "Test code";

}

//RJG - this is useful for debugging stuff with critters, and I'm a little bored of recoding it every time I need to print one to screen
void SimManager::debug_genome(quint64 genome)
{
    QString newGenome;
    for (int i = 0; i < 64; i++) {
        if (tweakers64[63 - i] & genome) newGenome.append("1");
        else newGenome.append("0");
    }
    qDebug() << newGenome;
}
