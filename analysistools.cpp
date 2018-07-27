/**
 * @file
 * Analysis Tools
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

#include "analysistools.h"
#include "simmanager.h"
#include "mainwindow.h"

#include <QApplication>
#include <QDataStream>
#include <QDebug>
#include <QFile>
#include <QList>
#include <QMap>
#include <QMessageBox>
#include <QStringList>
#include <QTextStream>
#include <QTime>

/**
 * @brief LoggedSpecies::LoggedSpecies
 */
LoggedSpecies::LoggedSpecies()
{
    lastGenome = 0;
    for (int i = 0; i < SCALE; i++) sizes[i] = 0; //means no data
}

/**
 * @brief StasisSpecies::StasisSpecies
 */
StasisSpecies::StasisSpecies()
{
    start = -1;
    end = -1;
    return;
}

/**
 * @brief AnalysisTools::AnalysisTools
 *
 * Text-generating tools to analyse files
 */
AnalysisTools::AnalysisTools()
{
    return;
}

/**
 * @brief AnalysisTools::dataFileNeededCheck
 * @param code
 * @return bool
 */
bool AnalysisTools::dataFileNeededCheck(int code)
{
    switch (code) {
    case ANALYSIS_TOOL_CODE_GENERATE_TREE:
    case ANALYSIS_TOOL_CODE_RATES_OF_CHANGE:
    case ANALYSIS_TOOL_CODE_STASIS:
    case ANALYSIS_TOOL_CODE_EXTINCT_ORIGIN:
        return true;

    default:
        return false;
    }
}

/**
 * @brief AnalysisTools::speciesRatesOfChange
 * @param filename
 * @return
 */
QString AnalysisTools::speciesRatesOfChange(QString filename)
{
    //Modified version of tree maker
    QMap <quint64, LoggedSpecies> speciesList;  //main list of species list, filed by Species ID

    QFile f(filename);
    if (f.open(QIODevice::ReadOnly)) {
        //file opened successfully

        quint64 lasttime = 0;

        //Need finish time to get scaling recorded correctly

        //read last line to find last time
        qint64 size = f.size(); //get file length
        f.seek(size - 500); // start 500 from end
        QTextStream in1(&f);
        QString s1;
        s1 = in1.readLine(); //header line
        while (!(s1.isNull())) {
            QStringList split_up;
            split_up = s1.split(',');
            lasttime = (quint64) (split_up[0].toULongLong());
            s1 = in1.readLine();
        }

        //OK, lasttime should be correct
        float timescale = (float)lasttime / (float)SCALE; //scale factor for working out the timeslice for diagram

        f.seek(0); //reset to start of file
        QTextStream in(&f);
        QString s;
        s = in.readLine(); //header
        s = in.readLine(); //first data line

        quint64 count = 0;
        while (!(s.isNull())) { //reads from first to last - which will be in date order
            QStringList split_up;
            split_up = s.split(',');
            //0 Time,1 Species_ID,2 Species_origin_time,3 Species_parent_ID,4 Species_current_size,5 Species_current_genome
            quint64 species_ID = (quint64) (split_up[1].toULongLong());

            //work out slot in 0-(SCALE-1)
            int xpos = (int)(((float)(split_up[0].toInt())) / timescale);
            if (xpos > (SCALE - 1)) xpos = SCALE - 1;


            if (speciesList.contains(species_ID)) { //if ID species already recorded, update details
                speciesList[species_ID].end = (split_up[0].toULongLong());
                int ssize = split_up[4].toInt();
                speciesList[species_ID].sizes[xpos] = ssize; //record last size in each slot, will be fine
                if (speciesList[species_ID].maxSize < ssize) speciesList[species_ID].maxSize = ssize;
                speciesList[species_ID].totalSize += (quint64) (split_up[4].toULongLong());
                speciesList[species_ID].occurrences++;
                speciesList[species_ID].genomes[xpos] = speciesList[species_ID].lastGenome = (quint64) (split_up[5].toULongLong());  //record all genomes - as yet do nothing with them except last
            } else { //not yet recorded
                LoggedSpecies spe;
                spe.start = (quint64) (split_up[2].toULongLong());
                speciesList[species_ID].sizes[xpos] = spe.end = (quint64) (split_up[0].toULongLong());
                spe.parent = (quint64) (split_up[3].toULongLong());
                spe.maxSize = split_up[4].toInt();
                spe.totalSize = (quint64) (split_up[4].toULongLong());
                spe.occurrences = 1;
                speciesList[species_ID].genomes[xpos] = spe.lastGenome = (quint64) (split_up[5].toULongLong());
                speciesList.insert(species_ID, spe);
            }

            count++;
            if (count % 1000 == 0)
                //do some display in status bar every 1000 iterations to show user that algorithm didn't die
            {
                quint64 thistime = (quint64) (split_up[0].toULongLong()); //record latest timestamp
                QString outstring;
                QTextStream out(&outstring);
                out << "Read to iteration " << thistime << " (" << ((thistime * 100) / lasttime) << "%)";
                MainWin->setStatusBarText(outstring);
                qApp->processEvents();
            }
            s = in.readLine(); //next line
        }
        f.close();

        //Write full species data out to report window
        QString OutputString;
        QTextStream out(&OutputString);
        out << endl << "=============================================================" << endl;
        out << endl << "Full species data " << endl;
        out << endl << "=============================================================" << endl;

        QMutableMapIterator<quint64, LoggedSpecies> i(
            speciesList);  //doesn't need to be mutable here, but reused later
        while (i.hasNext()) {
            i.next();
            quint64 ID = i.key();
            LoggedSpecies spe = i.value();
            int pval;
            if (spe.end != spe.start) pval = (100 * ((spe.end - spe.start) - spe.occurrences)) /
                                                 (spe.end - spe.start);
            else pval = 100;
            out << "Species: " << ID << ": " << spe.start << "-" << spe.end << " Parent " << spe.parent <<
                "  maxSize " << spe.maxSize << "  Av size " << (spe.totalSize / spe.occurrences) << "  %missing " <<
                100 - pval << endl;
        }

        //Now cull  extinct species without descendants

        count = 0;
        int speccount = speciesList.count();

        QSet <quint64> parents; //make a parents set - timesaving
        i.toFront();
        while (i.hasNext()) {
            i.next();
            parents.insert(i.value().parent);
        }

        i.toFront();
        while (i.hasNext()) {
            i.next();
            bool descendants = false;
            if (parents.contains(i.key())) descendants = true; //if it is in parents list it should survive cull
            //does it have descendants?

            if (i.value().end != lasttime
                    && descendants ==
                    false) //used to also have a term here to exlude short-lived species: && (i.value().end - i.value().start) < 400
                i.remove();

            count++;
            if (count % 100 == 0)
                //reporting to status bar
            {
                QString outstring;
                QTextStream out(&outstring);
                out << "Doing cull: done " << count << " species of " << speccount;
                MainWin->setStatusBarText(outstring);
                qApp->processEvents();
            }
        }

        //Output it
        out << endl << "=============================================================" << endl;
        out << endl << "Culled data (extinct species with no descendants removed)" << endl;
        out << endl << "=============================================================" << endl;
        i.toFront();
        while (i.hasNext()) {
            i.next();
            quint64 ID = i.key();
            LoggedSpecies spe = i.value();
            int pval;
            if (spe.end != spe.start) pval = (100 * ((spe.end - spe.start) - spe.occurrences)) /
                                                 (spe.end - spe.start);
            else pval = 100;
            out << "Species: " << ID << ": " << spe.start << "-" << spe.end << " Parent " << spe.parent <<
                "  maxSize " << spe.maxSize << "  Av size " << (spe.totalSize / spe.occurrences) << "  %missing " <<
                100 - pval << endl;
        }

        //Tree version reordered here, I just create magicList as a copy of culled list
        QList<quint64> magicList;

        i.toFront();
        while (i.hasNext()) {
            i.next();
            quint64 ID = i.key();
            magicList.append(ID);
        }

        //output the tree
        out << endl << "=============================================================" << endl;
        out << endl << "Species with change metrics (as csv) " << endl;
        out << endl << "=============================================================" << endl;

        //csv headers
        out << endl << "ID,cum_change,end_to_end_change,steps,";

        for (int k = 0; k < 20; k++)
            out << "size" << k << ",";

        for (int k = 0; k < 19; k++)
            out << "change" << k << ",";

        out << "change19" << endl;

        //work out averages etc
        for (int j = 0; j < magicList.count(); j++) {

            //work out average genome change

            int change = 0;
            int steps = 0;
            quint64 firstgenome = 0;
            bool flag = false;
            int start = -1;
            int tonextav = -1;
            int act_av_count = 0;
            int local_tot_size = 0;
            int local_tot_change = 0;


            for (int k = 1; k < SCALE; k++) {
                if (start != -1) tonextav--;
                if (speciesList[magicList[j]].sizes[k]) { //if this exists
                    if (flag == false) {
                        firstgenome = speciesList[magicList[j]].genomes[k];
                        flag = true;
                    }
                    if (speciesList[magicList[j]].sizes[k - 1]) { //if previous exists
                        //have a this and last to compare

                        if (start == -1) {
                            start = k;
                            tonextav = 5;
                            local_tot_size = 0;
                            local_tot_change = 0;
                            act_av_count = 0;
                        }
                        if (start != -1 && tonextav <= 0) {
                            tonextav = 5;
                            speciesList[magicList[j]].averageSizes.append((float)local_tot_size / (float)act_av_count);
                            speciesList[magicList[j]].averageChanges.append((float)local_tot_change / (float)act_av_count);
                            local_tot_size = 0;
                            local_tot_change = 0;
                            act_av_count = 0;
                            tonextav = 5;
                        }
                        quint64 thisgenome = speciesList[magicList[j]].genomes[k];
                        quint64 lastGenome = speciesList[magicList[j]].genomes[k - 1];
                        //work out difference

                        quint64 cg1x = thisgenome ^ lastGenome; //XOR the two to compare

                        //Coding half
                        quint32 g1xl = quint32(cg1x & ((quint64)65536 * (quint64)65536 - (quint64)1)); //lower 32 bits
                        int t1 = bitcounts[g1xl / (quint32)65536] +  bitcounts[g1xl & (quint32)65535];

                        //non-Coding half
                        quint32 g1xu = quint32(cg1x / ((quint64)65536 * (quint64)65536)); //upper 32 bits
                        t1 += bitcounts[g1xu / (quint32)65536] +  bitcounts[g1xu & (quint32)65535];

                        //if (t1>0) qDebug()<<"T1 not 0!"<<t1;
                        steps++;
                        change += t1;
                        local_tot_size += speciesList[magicList[j]].sizes[k];
                        local_tot_change += t1;
                        act_av_count++;
                    }
                }

            }

            quint64 cg1x = speciesList[magicList[j]].lastGenome ^ firstgenome; //XOR the two to compare

            //Coding half
            quint32 g1xl = quint32(cg1x & ((quint64)65536 * (quint64)65536 - (quint64)1)); //lower 32 bits
            int t1 = bitcounts[g1xl / (quint32)65536] +  bitcounts[g1xl & (quint32)65535];

            //non-Coding half
            quint32 g1xu = quint32(cg1x / ((quint64)65536 * (quint64)65536)); //upper 32 bits
            t1 += bitcounts[g1xu / (quint32)65536] +  bitcounts[g1xu & (quint32)65535];

            QString changestring1 = "'NA'";
            if (steps > 0) changestring1.sprintf("%0.5f", ((float)t1) / ((float)(steps + 1)));

            QString changestring = "'NA'";
            if (steps > 0)  changestring.sprintf("%0.5f", ((float)change) / ((float)steps));
            out << magicList[j] << "," << changestring << "," << changestring1 << "," << steps << ",";
            // qDebug()<<magicList[j]<<","<<changestring<<","<<changestring1<<","<<steps,<",";
            int countsizes = speciesList[magicList[j]].averageSizes.count();

            for (int k = 0; k < 20; k++) {
                QString outstringtemp;

                if (k >= countsizes) out << "0,";
                else {
                    outstringtemp.sprintf("%0.5f", speciesList[magicList[j]].averageSizes[k]);
                    out << outstringtemp << ",";
                }
            }
            for (int k = 0; k < 20; k++) {
                QString outstringtemp, commastring;
                if (k == 19) commastring = "";
                else commastring = ",";

                if (k >= countsizes) out << "0" << commastring;
                else {
                    outstringtemp.sprintf("%0.5f", speciesList[magicList[j]].averageChanges[k]);
                    out << outstringtemp << commastring;
                }
            }


            out << endl;

        }

        MainWin->setStatusBarText("Done");
        qApp->processEvents();

        return OutputString;
    } else {
        return "Can't open file";
    }

}

/**
 * @brief AnalysisTools::findClosestIndex
 * @param timeList
 * @param lookFor
 * @param slotWidth
 * @return
 */
int AnalysisTools::findClosestIndex(QList <quint64>timeList, float lookFor, float slotWidth)
{
    quint64 look_for_int = (quint64)(lookFor + .5);
    //check the before and after possibilities first
    if (look_for_int <= timeList[0]) //before start - should be impossible to be so
        return 0;

    //off end?
    if (look_for_int >= timeList.last()) {
        if ((float)(timeList.last() - look_for_int) >
                slotWidth) //off end, and more than one slot width away - so genuinely off end
            return -1;
        else return timeList.count() - 1;
    }

    //somewhere in middle!
    for (int ii = 0; ii < timeList.count() - 1; ii++) {
        if (timeList[ii] <= look_for_int && timeList[ii + 1] > look_for_int)
            //must be between ii and ii+1
        {
            if ((look_for_int - timeList[ii]) > (timeList[ii + 1] - look_for_int))
                //distance to ii more than distance to ii+1 - use ii+1
                return ii + 1;
            else
                return ii;
        }
    }
    return -2; //error code - shouldn't ever get here
}

/**
 * @brief AnalysisTools::stasis
 * @param filename
 * @param slotCount
 * @param percentileCut
 * @param qualifyingSlotCount
 * @return
 */
QString AnalysisTools::stasis(QString filename, int slotCount, float percentileCut, int qualifyingSlotCount)
{
    //Much of this copied from other tools - does loads I don't need, but does no harm. Will generate culled list
    qint64 maxspeciesID = -1;

    QString OutputString;
    QTextStream out(&OutputString);

    QMap <quint64, LoggedSpecies> speciesList;  //main list of species list, filed by Species ID


    QFile f(filename);
    if (f.open(QIODevice::ReadOnly)) {
        //file opened successfully

        quint64 lasttime = 0;

        //Need finish time to get scaling recorded correctly

        //read last line to find last time
        qint64 size = f.size(); //get file length
        f.seek(size - 500); // start 500 from end
        QTextStream in1(&f);
        QString s1;
        s1 = in1.readLine(); //header line
        s1 = in1.readLine(); //header line
        qDebug() << s1;
        while (!(s1.isNull())) {
            QStringList split_up;
            split_up = s1.split(',');
            lasttime = (quint64) (split_up[0].toULongLong());
            qDebug() << s1 << lasttime;
            s1 = in1.readLine();
        }

        //OK, lasttime should be correct
        float timescale = (float)lasttime / (float)SCALE; //scale factor for working out the timeslice for diagram


        f.seek(0); //reset to start of file
        QTextStream in(&f);
        QString s;
        s = in.readLine(); //header
        s = in.readLine(); //first data line

        quint64 count = 0;
        while (!(s.isNull())) { //reads from first to last - which will be in date order
            QStringList split_up;
            split_up = s.split(',');
            //0 Time,1 Species_ID,2 Species_origin_time,3 Species_parent_ID,4 Species_current_size,5 Species_current_genome
            quint64 species_ID = (quint64) (split_up[1].toULongLong());

            if (((qint64)species_ID) > maxspeciesID)
                maxspeciesID = (qint64)species_ID;
            //work out slot in 0-(SCALE-1)
            int xpos = (int)(((float)(split_up[0].toInt())) / timescale);
            if (xpos > (SCALE - 1)) xpos = SCALE - 1;


            if (speciesList.contains(species_ID)) { //if ID species already recorded, update details
                speciesList[species_ID].end = (split_up[0].toULongLong());
                int ssize = split_up[4].toInt();
                speciesList[species_ID].sizes[xpos] = ssize; //record last size in each slot, will be fine
                if (speciesList[species_ID].maxSize < ssize) speciesList[species_ID].maxSize = ssize;
                speciesList[species_ID].totalSize += (quint64) (split_up[4].toULongLong());
                speciesList[species_ID].occurrences++;
                speciesList[species_ID].genomes[xpos] = speciesList[species_ID].lastGenome = (quint64) (
                                                                                                 split_up[5].toULongLong());  //record all genomes - as yet do nothing with them except last
            } else { //not yet recorded
                LoggedSpecies spe;
                spe.start = (quint64) (split_up[2].toULongLong());
                speciesList[species_ID].sizes[xpos] = spe.end = (quint64) (split_up[0].toULongLong());
                spe.parent = (quint64) (split_up[3].toULongLong());
                spe.maxSize = split_up[4].toInt();
                spe.totalSize = (quint64) (split_up[4].toULongLong());
                spe.occurrences = 1;
                speciesList[species_ID].genomes[xpos] = spe.lastGenome = (quint64) (split_up[5].toULongLong());
                speciesList.insert(species_ID, spe);
            }

            count++;
            if (count % 1000 == 0)
                //do some display in status bar every 1000 iterations to show user that algorithm didn't die
            {
                quint64 thistime = (quint64) (split_up[0].toULongLong()); //record latest timestamp
                QString outstring;
                QTextStream out(&outstring);
                out << "Read to iteration " << thistime << " (" << ((thistime * 100) / lasttime) << "%)";
                MainWin->setStatusBarText(outstring);
                qApp->processEvents();
            }
            s = in.readLine(); //next line
        }
        //f.close();

        QMutableMapIterator<quint64, LoggedSpecies> i(
            speciesList);  //doesn't need to be mutable here, but reused later
        while (i.hasNext()) {
            i.next();
            //quint64 ID=i.key();
            LoggedSpecies spe = i.value();
            int pval;
            if (spe.end != spe.start) pval = (100 * ((spe.end - spe.start) - spe.occurrences)) /
                                                 (spe.end - spe.start);
            else pval = 100;
            //out << "Species: "<<ID << ": " << spe.start << "-"<<spe.end<<" Parent "<<spe.parent<<"  maxSize "<<spe.maxSize<<"  Av size "<<(spe.totalSize/spe.occurrences)<< "  %missing "<<100-pval<< endl;

            //ARTS - compiler warning supression
            Q_UNUSED(pval);
        }

        //Now cull  extinct species without descendants

        count = 0;
        int speccount = speciesList.count();

        QSet <quint64> parents; //make a parents set - timesaving
        i.toFront();
        while (i.hasNext()) {
            i.next();
            parents.insert(i.value().parent);
        }

        i.toFront();
        while (i.hasNext()) {
            i.next();
            bool descendants = false;
            if (parents.contains(i.key())) descendants = true; //if it is in parents list it should survive cull
            //does it have descendants?

            if (i.value().end != lasttime
                    && descendants ==
                    false) //used to also have a term here to exlude short-lived species: && (i.value().end - i.value().start) < 400
                i.remove();

            count++;
            if (count % 100 == 0)
                //reporting to status bar
            {
                QString outstring;
                QTextStream out(&outstring);
                out << "Doing cull: done " << count << " species of " << speccount;
                MainWin->setStatusBarText(outstring);
                qApp->processEvents();
            }
        }

        //Output it

        i.toFront();
        while (i.hasNext()) {
            i.next();
            //quint64 ID=i.key();
            LoggedSpecies spe = i.value();
            int pval;
            if (spe.end != spe.start) pval = (100 * ((spe.end - spe.start) - spe.occurrences)) /
                                                 (spe.end - spe.start);
            else pval = 100;
            //out << "Species: "<<ID << ": " << spe.start << "-"<<spe.end<<" Parent "<<spe.parent<<"  maxSize "<<spe.maxSize<<"  Av size "<<(spe.totalSize/spe.occurrences)<< "  %missing "<<100-pval<<endl;

            //ARTS - compiler warning supression
            Q_UNUSED(pval);
        }

        //Tree version reordered here, I just create magicList as a copy of culled list
        QList <StasisSpecies *> stasis_species_list;

        //also need quick lookup table for stasis_species_list ID from species_ID
        QList <int> species_lookup;
        for (qint64 ii = 0; ii <= maxspeciesID; ii++) species_lookup.append(-1);

        int apos = 0;
        i.toFront();
        while (i.hasNext()) {
            i.next();
            quint64 ID = i.key();
            StasisSpecies *newspecies = new StasisSpecies;
            newspecies->ID = ID;
            stasis_species_list.append(newspecies);
            species_lookup[(int)ID] = apos;
            apos++;
        }

        //End copied code. stasis_species_list is list of culled species IDs in new datastructure

        //go back over data. For all useful species, start building my new strucure
        f.seek(0);
        s = in.readLine(); //header
        s = in.readLine(); //first data line

        count = 0;
        while (!(s.isNull())) { //reads from first to last - which will be in date order
            QStringList split_up;
            split_up = s.split(',');
            //0 Time,1 Species_ID,2 Species_origin_time,3 Species_parent_ID,4 Species_current_size,5 Species_current_genome
            quint64 species_ID = (quint64) (split_up[1].toULongLong());

            if (species_lookup[(int)species_ID] != -1) {
                //it's a real one
                StasisSpecies *this_species = stasis_species_list[species_lookup[(int)species_ID]];

                this_species->end = (qint64) (split_up[0].toLongLong());  //always update end

                if (this_species->start == -1) this_species->start = (qint64) (split_up[0].toLongLong());
                this_species->genomes.append(((quint64) (split_up[5].toULongLong())));
                this_species->genomeSampleTimes.append(this_species->end);
            }
            s = in.readLine(); //next line
        }

        //calculate length of my slots
        //1. build list of all lengths
        //2. sort it
        //3. use percentile to find correct length.

        QList<qint64> sortable_lengths;
        for (int ii = 0; ii < stasis_species_list.count(); ii++)
            sortable_lengths.append(stasis_species_list[ii]->end - stasis_species_list[ii]->start);

        qSort(sortable_lengths);

        int percentilepos = (int)((float)sortable_lengths.count() * percentileCut);
        qint64 length_species = sortable_lengths[percentilepos];
        float slot_length = (float)length_species / (float)slotCount;


        int nan_cull = 0;
        //Now second pass. For each species, from start_time on, work out average change in each slot.
        //do this by finding the samples closest to start and end time. Compare them, work out actual time between samples,
        //so work out average change
        for (int ii = 0; ii < stasis_species_list.count(); ii++) {
            if (ii % 10 == 0)
                //do some display in status bar every 10 iterations to show user that algorithm didn't die
            {
                QString outstring;
                QTextStream out(&outstring);
                out << "Second pass " << ii << " (out of " << stasis_species_list.count() << ")";
                MainWin->setStatusBarText(outstring);
                qApp->processEvents();
            }

            StasisSpecies *this_species = stasis_species_list[ii];

            float slot_start_pos = (float) this_species->start;

            for (int ii = 0; ii < slotCount; ii++) {
                float slot_end_pos = slot_start_pos + slot_length;

                //find closest start point
                int closestindexend = findClosestIndex(this_species->genomeSampleTimes, slot_end_pos,
                                                       slot_length);

                if (closestindexend == -2) {
                    qDebug() <<
                             "Something that should never have returned a -2 has returned a -2. Bugger. Read the source code!";
                    break;
                }

                if (closestindexend == -1)
                    break;

                quint64 genome2 = this_species->genomes[closestindexend];

                int closestindexstart = findClosestIndex(this_species->genomeSampleTimes, slot_start_pos,
                                                         slot_length);

                if (closestindexstart == -2) {
                    qDebug() <<
                             "Something that should never have returned a -2 has returned a -2. Arse. Read the source code!";
                    break;
                }

                if (closestindexstart == -1) {
                    qDebug() <<
                             "Something that should never have returned a -1 has returned a -1. Ooops. Read the source code!";
                    break;
                }

                if (closestindexend == closestindexstart) { //gaps mean going to get a div/0
                    this_species->resampledAverageGenomeChanges.clear();
                    nan_cull++;
                    break;
                }
                quint64 genome1 = this_species->genomes[closestindexstart];

                quint64 cg1x = genome1 ^ genome2; //XOR the two to compare

                quint32 g1xl = quint32(cg1x & ((quint64)65536 * (quint64)65536 - (quint64)1)); //lower 32 bits
                int t1 = bitcounts[g1xl / (quint32)65536] +  bitcounts[g1xl & (quint32)65535];
                quint32 g1xu = quint32(cg1x / ((quint64)65536 * (quint64)65536)); //upper 32 bits
                t1 += bitcounts[g1xu / (quint32)65536] +  bitcounts[g1xu & (quint32)65535];

                float avdiff = ((float)t1) / ((float)this_species->genomeSampleTimes[closestindexend] -
                                              this_species->genomeSampleTimes[closestindexstart]);

                this_species->resampledAverageGenomeChanges.append(avdiff);

                slot_start_pos = slot_end_pos; // for next iteration
            }

        }

        //csv headers
        out << "ID,start,end,";
        for (int k = 0; k < slotCount - 1; k++)
            out << "change" << k << ",";

        out << "change" << slotCount - 1 << endl;

        //csv data
        int countall = stasis_species_list.count();
        int countshown = 0;
        for (int k = 0; k < stasis_species_list.count(); k++) {
            StasisSpecies *this_species = stasis_species_list[k];
            if (this_species->resampledAverageGenomeChanges.count() >= qualifyingSlotCount) {
                countshown++;
                out << this_species->ID << "," << this_species->start << "," << this_species->end;
                for (int ii = 0; ii < this_species->resampledAverageGenomeChanges.count(); ii++) {
                    out << "," << this_species->resampledAverageGenomeChanges[ii];
                }
                out << endl;
            }
        }
        out << endl << endl;
        out << "Of " << countall << " post-cull species, showing " << countshown << ", removed " <<
            (countall - countshown) << " of which " << nan_cull <<
            " were divide by zero errors - data too gappy";

        MainWin->setStatusBarText("Done");
        qApp->processEvents();

        qDeleteAll(stasis_species_list);
        return OutputString;
    } else {
        return "Can't open file";
    }

}

/**
 * @brief AnalysisTools::extinctOrigin
 * @param filename
 * @return
 */
QString AnalysisTools::extinctOrigin(QString filename)
{
    QMap <quint64, LoggedSpecies> speciesList;  //main list of species list, filed by Species ID

    QFile f(filename);
    if (f.open(QIODevice::ReadOnly)) {
        quint64 lasttime = 0;

        //Need finish time to get scaling recorded correctly

        //read last line to find last time
        qint64 size = f.size(); //get file length
        f.seek(size - 500); // start 500 from end
        QTextStream in1(&f);
        QString s1;
        s1 = in1.readLine(); //header line
        while (!(s1.isNull())) {
            QStringList split_up;
            split_up = s1.split(',');
            lasttime = (quint64) (split_up[0].toULongLong());
            s1 = in1.readLine();
        }

        //OK, lasttime should be correct
        float timescale = (float)lasttime / (float)SCALE; //scale factor for working out the timeslice for diagram


        f.seek(0); //reset to start of file
        QTextStream in(&f);
        QString s;
        s = in.readLine(); //header
        s = in.readLine(); //first data line

        QList<int> RealSpeciesCounts; //for counting the actual species/look data
        QList<int> RealSpeciesTimeSlots;

        int LastTime = -1;
        int RealCount = 0;

        quint64 count = 0;
        while (!(s.isNull())) { //reads from first to last - which will be in date order
            QStringList split_up;
            split_up = s.split(',');
            //0 Time,1 Species_ID,2 Species_origin_time,3 Species_parent_ID,4 Species_current_size,5 Species_current_genome

            if (split_up[0].toInt() == LastTime) { //same timeslot, keep counting
                RealCount++;
            } else {
                if (LastTime != -1) {
                    //record old if not first time
                    RealSpeciesCounts.append(RealCount);
                    RealSpeciesTimeSlots.append(LastTime);
                }
                LastTime = split_up[0].toInt(); //record timeslot
                RealCount = 1;
            }


            quint64 species_ID = (quint64) (split_up[1].toULongLong());

            //work out slot in 0-(SCALE-1)
            int xpos = (int)(((float)(split_up[0].toInt())) / timescale);
            if (xpos > (SCALE - 1)) xpos = SCALE - 1;



            if (speciesList.contains(species_ID)) { //if ID species already recorded, update details
                speciesList[species_ID].end = (split_up[0].toULongLong());
                int ssize = split_up[4].toInt();
                speciesList[species_ID].sizes[xpos] = ssize; //record last size in each slot, will be fine
                if (speciesList[species_ID].maxSize < ssize) speciesList[species_ID].maxSize = ssize;
                speciesList[species_ID].totalSize += (quint64) (split_up[4].toULongLong());
                speciesList[species_ID].occurrences++;
                speciesList[species_ID].genomes[xpos] = speciesList[species_ID].lastGenome = (quint64) (split_up[5].toULongLong());  //record all genomes - as yet do nothing with them except last
            } else { //not yet recorded
                LoggedSpecies spe;
                spe.start = (quint64) (split_up[2].toULongLong());
                speciesList[species_ID].sizes[xpos] = spe.end = (quint64) (split_up[0].toULongLong());
                spe.parent = (quint64) (split_up[3].toULongLong());
                spe.maxSize = split_up[4].toInt();
                spe.totalSize = (quint64) (split_up[4].toULongLong());
                spe.occurrences = 1;
                speciesList[species_ID].genomes[xpos] = spe.lastGenome = (quint64) (split_up[5].toULongLong());
                speciesList.insert(species_ID, spe);
            }

            count++;
            if (count % 1000 == 0)
                //do some display in status bar every 1000 iterations to show user that algorithm didn't die
            {
                quint64 thistime = (quint64) (split_up[0].toULongLong()); //record latest timestamp
                QString outstring;
                QTextStream out(&outstring);
                out << "Read to iteration " << thistime << " (" << ((thistime * 100) / lasttime) << "%)";
                MainWin->setStatusBarText(outstring);
                qApp->processEvents();
            }
            s = in.readLine(); //next line
        }
        f.close();

        //now work out origins etc

        QList<int> origincounts;
        QList<int> extinctcounts;
        QList<int> alivecounts;
        QList<int> alive_big_counts;

        for (int i = 0; i < SCALE; i++) {
            int count = 0;
            int ecount = 0;
            int acount = 0;
            int bcount = 0;
            foreach (LoggedSpecies spec, speciesList) {
                int xpos = (int)(((float)(spec.start)) / timescale);
                if (xpos > (SCALE - 1)) xpos = SCALE - 1;
                if (xpos == i) count++;

                int xpos2 = (int)(((float)(spec.end)) / timescale);
                if (xpos2 > (SCALE - 1)) xpos2 = SCALE - 1;
                if (xpos2 == i) ecount++;

                if (xpos <= i && xpos2 >= i) acount++;
                if (xpos <= i && xpos2 >= i && spec.maxSize > 20) bcount++;
            }
            extinctcounts.append(ecount);
            origincounts.append(count);
            alivecounts.append(acount);
            alive_big_counts.append(bcount);
        }

        //calc av species alive
        for (int i = 0; i < RealSpeciesTimeSlots.count(); i++) {
            int xpos = (int)(((float)(RealSpeciesTimeSlots[i])) / timescale);
            if (xpos > (SCALE - 1)) xpos = SCALE - 1;
            RealSpeciesTimeSlots[i] = xpos;
        }

        float avspeciescounts[SCALE];
        for (int i = 0; i < SCALE; i++) avspeciescounts[i] = -1.0;

        int countspecies = 1;
        int total = RealSpeciesCounts[0];
        int last = RealSpeciesTimeSlots[0];
        for (int i = 1; i < RealSpeciesTimeSlots.count(); i++) {
            if (RealSpeciesTimeSlots[i] == last) {
                countspecies++;
                total += RealSpeciesCounts[i];
            } else {
                //next slot
                avspeciescounts[last] = (float)total / (float)countspecies;
                qDebug() << total << countspecies;
                countspecies = 1;
                last = RealSpeciesTimeSlots[i];
                total = RealSpeciesCounts[i];
            }
        }


        QString outstring;
        QTextStream out(&outstring);

        out << "Extinctions,Originations,SpeciesCount,AvSpeciesCount,BigSpeciesCount" << endl;
        //output
        for (int i = 0; i < SCALE; i++) {
            out << extinctcounts[i] << "," << origincounts[i] << "," << alivecounts[i] << "," <<
                avspeciescounts[i] << "," << alive_big_counts[i] << endl;
        }

        return outstring;
    } else return "Could not open file";
}

/**
 * @brief AnalysisTools::generateTree
 *
 * Takes a log file generated by the species logging system
 * Generates output to the report dock
 * Currently assumes start at time 0, with species 1
 * 1. Generates list of all species to have lived, together with time-range, parent, etc
 * 2. Culls this list to exclude all species than went extinct without descendants
 * 3. Generates a phylogram showing ranges of species and their parent
 *
 * @param filename
 * @return
 */
QString AnalysisTools::generateTree(QString filename)
{
    QMap <quint64, LoggedSpecies> speciesList;  //main list of species list, filed by Species ID

    QFile f(filename);
    if (f.open(QIODevice::ReadOnly)) {
        //file opened successfully

        quint64 lasttime = 0;

        //Need finish time to get scaling recorded correctly

        //read last line to find last time
        qint64 size = f.size(); //get file length
        f.seek(size - 500); // start 500 from end
        QTextStream in1(&f);
        QString s1;
        s1 = in1.readLine(); //header line
        while (!(s1.isNull())) {
            QStringList split_up;
            split_up = s1.split(',');
            lasttime = (quint64) (split_up[0].toULongLong());
            s1 = in1.readLine();
        }

        //OK, lasttime should be correct
        float timescale = (float)lasttime / (float)SCALE; //scale factor for working out the timeslice for diagram

        f.seek(0); //reset to start of file
        QTextStream in(&f);
        QString s;
        s = in.readLine(); //header
        s = in.readLine(); //first data line

        quint64 count = 0;
        while (!(s.isNull())) { //reads from first to last - which will be in date order
            QStringList split_up;
            split_up = s.split(',');
            //0 Time,1 Species_ID,2 Species_origin_time,3 Species_parent_ID,4 Species_current_size,5 Species_current_genome
            quint64 species_ID = (quint64) (split_up[1].toULongLong());

            //work out slot in 0-(SCALE-1)
            int xpos = (int)(((float)(split_up[0].toInt())) / timescale);
            if (xpos > (SCALE - 1)) xpos = SCALE - 1;

            if (speciesList.contains(species_ID)) { //if ID species already recorded, update details
                speciesList[species_ID].end = (split_up[0].toULongLong());
                int ssize = split_up[4].toInt();
                speciesList[species_ID].sizes[xpos] = ssize; //record last size in each slot, will be fine
                if (speciesList[species_ID].maxSize < ssize) speciesList[species_ID].maxSize = ssize;
                speciesList[species_ID].totalSize += (quint64) (split_up[4].toULongLong());
                speciesList[species_ID].occurrences++;
                speciesList[species_ID].genomes[xpos] = speciesList[species_ID].lastGenome = (quint64) (split_up[5].toULongLong());  //record all genomes - as yet do nothing with them except last
            } else { //not yet recorded
                LoggedSpecies spe;
                spe.start = (quint64) (split_up[2].toULongLong());
                speciesList[species_ID].sizes[xpos] = spe.end = (quint64) (split_up[0].toULongLong());
                spe.parent = (quint64) (split_up[3].toULongLong());
                spe.maxSize = split_up[4].toInt();
                spe.totalSize = (quint64) (split_up[4].toULongLong());
                spe.occurrences = 1;
                speciesList[species_ID].genomes[xpos] = spe.lastGenome = (quint64) (split_up[5].toULongLong());
                speciesList.insert(species_ID, spe);
            }

            count++;
            //do some display in status bar every 1000 iterations to show user that algorithm didn't die
            if (count % 1000 == 0) {
                quint64 thistime = (quint64) (split_up[0].toULongLong()); //record latest timestamp
                QString outstring;
                QTextStream out(&outstring);
                out << "Read to iteration " << thistime << " (" << ((thistime * 100) / lasttime) << "%)";
                MainWin->setStatusBarText(outstring);
                qApp->processEvents();
            }
            s = in.readLine(); //next line
        }
        f.close();

        //Write full species data out to report window
        QString OutputString;
        QTextStream out(&OutputString);
        out << endl << "=============================================================" << endl;
        out << endl << "Full species data " << endl;
        out << endl << "=============================================================" << endl;

        QMutableMapIterator<quint64, LoggedSpecies> i(speciesList);  //doesn't need to be mutable here, but reused later
        while (i.hasNext()) {
            i.next();
            quint64 ID = i.key();
            LoggedSpecies spe = i.value();
            int pval;
            if (spe.end != spe.start) pval = (100 * ((spe.end - spe.start) - spe.occurrences)) /
                                                 (spe.end - spe.start);
            else pval = 100;
            out << "Species: " << ID << ": " << spe.start << "-" << spe.end << " Parent " << spe.parent <<
                "  maxSize " << spe.maxSize << "  Av size " << (spe.totalSize / spe.occurrences) << "  %missing " <<
                100 - pval << endl;
        }

        //Now cull  extinct species without descendants

        count = 0;
        int speccount = speciesList.count();

        QSet <quint64> parents; //make a parents set - timesaving
        i.toFront();
        while (i.hasNext()) {
            i.next();
            parents.insert(i.value().parent);
        }

        i.toFront();
        while (i.hasNext()) {
            i.next();
            bool descendants = false;
            if (parents.contains(i.key())) descendants = true; //if it is in parents list it should survive cull
            //does it have descendants?

            if (i.value().end != lasttime && descendants == false) //used to also have a term here to exlude short-lived species: && (i.value().end - i.value().start) < 400
                i.remove();

            count++;
            //reporting to status bar
            if (count % 100 == 0) {
                QString outstring;
                QTextStream out(&outstring);
                out << "Doing cull: done " << count << " species of " << speccount;
                MainWin->setStatusBarText(outstring);
                qApp->processEvents();
            }
        }

        //Output it
        out << endl << "=============================================================" << endl;
        out << endl << "Culled data (extinct species with no descendants removed)" << endl;
        out << endl << "=============================================================" << endl;
        i.toFront();
        while (i.hasNext()) {
            i.next();
            quint64 ID = i.key();
            LoggedSpecies spe = i.value();
            int pval;
            if (spe.end != spe.start) pval = (100 * ((spe.end - spe.start) - spe.occurrences)) /
                                                 (spe.end - spe.start);
            else pval = 100;
            out << "Species: " << ID << ": " << spe.start << "-" << spe.end << " Parent " << spe.parent <<
                "  maxSize " << spe.maxSize << "  Av size " << (spe.totalSize / spe.occurrences) << "  %missing " <<
                100 - pval << endl;
        }

        //now do my reordering magic - magicList ends up as a list of species IDs in culled list, but in a sensible order for tree output
        QList<quint64> magicList;

        MainWin->setStatusBarText("Starting list reordering");
        qApp->processEvents();

        makeListRecursive(&magicList, &speciesList, 1, 0); //Recursively sorts culled speciesList into magicList. args are list pointer, ID

        //Display code for this list, now removed
        /*
        out<<endl<<"============================================================="<<endl;
        out<<endl<<"Sorted by parents"<<endl;
        out<<endl<<"============================================================="<<endl;

        foreach (quint64 ID, magicList)
        {
            out << "ID: "<<ID<<" Parent "<<speciesList[ID].parent<<" Genome: "<<speciesList[ID].lastGenome<<endl;
        }

        */

        //output the tree
        out << endl << "=============================================================" << endl;
        out << endl << "Tree" << endl;
        out << endl << "=============================================================" << endl;


        MainWin->setStatusBarText("Calculating Tree");
        qApp->processEvents();

        /*
           grid for output - holds output codes for each point in grid, which is SCALE wide and speciesList.count()*2 high (includes blank lines, one per species).
           Codes are:  0: space  1: -  2: +  3: |  4: /  5: \
        */
        QVector <QVector <int> > output_grid;

        QVector <int> blankline; //useful later
        for (int j = 0; j < SCALE; j++)
            blankline.append(0);

        //put in the horizontals
        foreach (quint64 ID, magicList) {
            //do horizontal lines
            QVector <int> line;
            for (int j = 0; j < SCALE; j++) { //+1 in case of rounding cockups
                //work out what start and end time this means
                quint64 starttime = (quint64)((float)j * timescale);
                quint64 endtime = (quint64)((float)(j + 1) * timescale);

                if (speciesList[ID].start <= endtime && speciesList[ID].end >= starttime) {
                    //do by size
                    line.append(1);
                } else line.append(0);
            }

            output_grid.append(line);
            //append a blank line
            output_grid.append(blankline);
        }

        //now the connectors, i.e. the verticals
        int myline = 0;
        foreach (quint64 ID, magicList) {
            //find parent
            int parent = speciesList[ID].parent;

            if (parent > 0) {
                //find parent's line number
                int pline = magicList.indexOf(parent) * 2;
                int xpos = (int)(((float)speciesList[ID].start) / timescale);
                if (xpos > (SCALE - 1)) xpos = SCALE - 1;
                output_grid[pline][xpos] = 2;
                if (pline > myline) { // below
                    for (int j = myline + 1; j < pline; j++) {
                        output_grid[j][xpos] = 3;
                    }
                    output_grid[myline][xpos] = 4;
                } else { //above
                    for (int j = pline + 1; j < myline; j++) {
                        output_grid[j][xpos] = 3;
                    }
                    output_grid[myline][xpos] = 5;
                }
            }
            myline++;
            myline++;
        }

        //Convert grid to ASCII output
        for (int j = 0; j < output_grid.count(); j++) {
            for (int k = 0; k < SCALE; k++) {
                if (output_grid[j][k] == 0) out << " ";
                if (output_grid[j][k] == 1) out << "-"; //Possibly size characters: from small to big: . ~ = % @
                if (output_grid[j][k] == 2) out << "+";
                if (output_grid[j][k] == 3) out << "|";
                if (output_grid[j][k] == 4) out << "/";
                if (output_grid[j][k] == 5) out << "\\";
            }
            QString extra;

            if (j % 2 == 0) {
                out << "ID:" << magicList[j / 2] << endl;
            } else out << endl;
        }

        MainWin->setStatusBarText("Done tree");
        qApp->processEvents();

        //Finally output genomes of all extant taxa - for cladistic comparison originally
        out << endl << "=============================================================" << endl;
        out << endl << "Genomes for extant taxa" << endl;
        out << endl << "=============================================================" << endl;

        int tcount = 0;
        i.toFront();
        while (i.hasNext()) {
            i.next();
            quint64 ID = i.key();
            LoggedSpecies spe = i.value();
            if (spe.end == lasttime) {
                tcount++;
                out << "Genome: " << returnBinary(spe.lastGenome) << "  ID: " << ID << endl;
            }
        }

        out << endl;
        out << "Taxa: " << tcount << endl;

        return OutputString;
    } else {
        return "Can't open file";
    }
}

/**
 * @brief AnalysisTools::returnBinary
 * @param genome
 * @return
 */
QString AnalysisTools::returnBinary(quint64 genome)
{
    QString s;
    QTextStream sout(&s);

    for (int j = 0; j < 64; j++)
        if (tweakers64[63 - j] & genome) sout << "1";
        else sout << "0";

    return s;
}

/**
 * @brief AnalysisTools::makeListRecursive
 *
 * This does the re-ordering for graph
 * Trick is to insert children of each taxon alternatively on either side of it - but do that recursively so each
 * child turns into an entire clade, if appropriate
 *
 * @param magicList
 * @param speciesList
 * @param ID
 * @param insertPosition
 */
void AnalysisTools::makeListRecursive(QList<quint64> *magicList, QMap <quint64, LoggedSpecies> *speciesList, quint64 ID, int insertPosition)
{
    //insert this item into the magicList at requested pos
    magicList->insert(insertPosition, ID);
    //find children
    QMapIterator <quint64, LoggedSpecies> i(*speciesList);

    bool before = false; //alternate before/after
    i.toFront();
    while (i.hasNext()) {
        i.next();
        if (i.value().parent == ID) {
            //find insert position
            int pos = magicList->indexOf(ID);
            if (before)
                makeListRecursive(magicList, speciesList, i.key(), pos);
            else
                makeListRecursive(magicList, speciesList, i.key(), pos + 1);

            before = !before;
        }
    }
}

/**
 * @brief AnalysisTools::countPeaks
 * @param r
 * @param g
 * @param b
 * @return
 */
QString AnalysisTools::countPeaks(int r, int g, int b)
{
    //return "CP";
    // for a particular colours - go through ALL genomes, work out fitness.
    quint8 env[3];
    quint32 fits[96];

    for (int i = 0; i < 96; i++) fits[i] = 0;

    env[0] = (quint8)r;
    env[1] = (quint8)g;
    env[2] = (quint8)b;

    QString s;
    QTextStream out(&s);

    out << "Fitness counts for red=" << r << " green=" << g << " blue=" << b << endl << endl;

    quint64 max = (qint64)65536 * (qint64)65536;

    //quint64 max = 1000000;
    for (quint64 genome = 0; genome < max; genome++) {
        Critter c;
        c.initialise((quint32)genome, env, 0, 0, 0, 0);
        fits[c.fitness]++;

        if (!(genome % 6553600)) {
            QString s2;
            QTextStream out2(&s2);
            out2 << (double)genome*(double)100 / (double)max << "% done...";

            MainWin->setStatusBarText(s2);
            qApp->processEvents();
            /*for (int i=0; i<=settleTolerance; i++)
            {
                QString s2;
                QTextStream out2(&s2);

                out2<<i<<" found: "<<fits[i];
                ui->plainTextEdit->appendPlainText(s2);
                qApp->processEvents();
            }*/
        }
    }
    //done - write out

    for (int i = 0; i <= settleTolerance; i++) {
        out << i << "," << fits[i] << endl;
    }


    return s;
}

/**
 * @brief AnalysisTools::makeNewick
 *
 * Output both for docker, and for species log.
 *
 * @param root
 * @param minSpeciesSize
 * @param allowExclude
 * @return
 */
QString AnalysisTools::makeNewick(LogSpecies *root, quint64 minSpeciesSize, bool allowExclude)
{
    ids = 0;
    minspeciessize = minSpeciesSize;
    allowExcludeWithDescendants = allowExclude;

    if (root)
        return root->newickstring(0, 0, true);
    else
        return "ERROR - NO PHYLOGENY DATA";
}

/**
 * @brief AnalysisTools::dumpData
 * @param root
 * @param minSpeciesSize
 * @param allowExclude
 * @return
 */
QString AnalysisTools::dumpData(LogSpecies *root, quint64 minSpeciesSize, bool allowExclude)
{
    ids = 0;


    quint64 ID;
    LogSpecies *parent;
    quint64 timeOfFirstAppearance;
    quint64 timeOfLastAppearance;
    QList<LogSpeciesDataItem *>data_items;
    QList<LogSpecies *>children;
    quint32 maxSize;


    quint64 generation;
    quint64 sample_genome;
    quint32 size; //number of critters
    quint32 genomic_diversity; //number of genomes
    quint16 cells_occupied; //number of cells found in - 1 (as real range is 1-65536, to fit in 0-65535)
    quint8 geographical_range; //max distance between outliers
    quint8 centroid_range_x; //mean of x positions
    quint8 centroid_range_y; //mean of y positions
    quint16 mean_fitness; //mean of all critter fitnesses, stored as x1000
    quint8 min_env[3]; //min red, green, blue found in
    quint8 max_env[3]; //max red, green, blue found in
    quint8 mean_env[3]; //mean environmental colour found in


    minspeciessize = minSpeciesSize;
    allowExcludeWithDescendants = allowExclude;
    if (root)
        return "ID,ParentID,generation,size,sample_genome,sample_genome_binary,diversity,cells_occupied,geog_range,centroid_x,centroid_y,mean_fit,min_env_red,min_env_green,min_env_blue,max_env_red,max_env_green,max_env_blue,mean_env_red,mean_env_green,mean_env_blue\n"
               +
               root->dump_data(0, 0, true);
    else
        return "ERROR - NO PHYLOGENY DATA";

    //recurse over species,

    //ARTS - compiler warning supression
    Q_UNUSED(ID);
    Q_UNUSED(parent);
    Q_UNUSED(timeOfFirstAppearance);
    Q_UNUSED(timeOfLastAppearance);
    Q_UNUSED(maxSize);
    Q_UNUSED(generation);
    Q_UNUSED(sample_genome);
    Q_UNUSED(size);
    Q_UNUSED(genomic_diversity);
    Q_UNUSED(cells_occupied);
    Q_UNUSED(geographical_range);
    Q_UNUSED(centroid_range_x);
    Q_UNUSED(centroid_range_y);
    Q_UNUSED(mean_fitness);
    Q_UNUSED(min_env);
    Q_UNUSED(max_env);
    Q_UNUSED(mean_env);
}
