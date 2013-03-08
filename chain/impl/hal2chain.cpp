/*
 * Copyright (C) 2012 by Glenn Hickey (hickey@soe.ucsc.edu)
 *
 * Released under the MIT license, see LICENSE.txt
 */

#include <cstdlib>
#include <iostream>
#include <fstream>
#include "halChain.h"

using namespace std;
using namespace hal;

int main(int argc, char** argv)
{
  CLParserPtr optionsParser = hdf5CLParserInstance();
  optionsParser->setDescription("Rertrieve chain (pairwise alignment) "
                                "information from a hal database");
  optionsParser->addArgument("halFile", "path to hal file to analyze");
  optionsParser->addArgument("genome", "(query) genome to process");
  optionsParser->addOption("sequence", "sequence name in query genome ("
                           "all sequences if not specified)", "\"\"");
  optionsParser->addOption("start", "start position in query genome", 0);
  optionsParser->addOption("length", "maximum length of chain to output.", 0);
  optionsParser->addOption("chainFile", "path for output file.  stdout if not"
                           " specified", "\"\"");
  optionsParser->addOption("maxGap", 
                           "maximum indel length to be considered a gap.  Gaps "
                           " can be nested within other rearrangements.", 
                           20);
  

  string halPath;
  string chainPath;
  string genomeName;
  string sequenceName;
  hal_size_t start;
  hal_size_t length;
  hal_size_t maxGap;
  try
  {
    optionsParser->parseOptions(argc, argv);
    halPath = optionsParser->getArgument<string>("halFile");
    genomeName = optionsParser->getArgument<string>("genome");
    sequenceName = optionsParser->getOption<string>("sequence");
    start = optionsParser->getOption<hal_size_t>("start");
    length = optionsParser->getOption<hal_size_t>("length");
    chainPath = optionsParser->getOption<string>("chainFile");
    maxGap = optionsParser->getOption<hal_size_t>("maxGap");
  }
  catch(exception& e)
  {
    cerr << e.what() << endl;
    optionsParser->printUsage(cerr);
    exit(1);
  }
  try
  {
    AlignmentConstPtr alignment = openHalAlignmentReadOnly(halPath,
                                                           optionsParser);
    
    
    const Genome* genome = alignment->openGenome(genomeName);
    if (genome == NULL)
    {
      throw hal_exception(string("Genome not found: ") + genomeName);
    }
    hal_index_t endPosition = 
       length > 0 ? start + length : genome->getSequenceLength();

    const Sequence* sequence = NULL;
    if (sequenceName != "\"\"")
    {
      sequence = genome->getSequence(sequenceName);
      if (sequence == NULL)
      {
        throw hal_exception(string("Sequence not found: ") + sequenceName);
      }
      start += sequence->getStartPosition();
      endPosition =  
         length > 0 ? start + length : sequence->getSequenceLength();
    }

    ofstream ofile;
    ostream& outStream = chainPath == "\"\"" ? cout : ofile;
    if (chainPath != "\"\"")
    {
      ofile.open(chainPath.c_str());
      if (!ofile)
      {
        throw hal_exception(string("Error opening output file ") + 
                            chainPath);
      }
    }

    TopSegmentIteratorConstPtr top = genome->getTopSegmentIterator();
    top->toSite(start, false);
    // do slicing here;
    
    GappedTopSegmentIteratorConstPtr gtop = 
       genome->getGappedTopSegmentIterator(top->getArrayIndex(), maxGap);

    // need to review!
    Chain chain;
    while (gtop->getLeft()->getStartPosition() < endPosition &&
           gtop->getRightArrayIndex() < genome->getNumTopSegments())
    {
      if (gtop->hasParent() == true)
      {
        hal_offset_t leftOffset = 0;
        if (start > gtop->getStartPosition() 
            && start < gtop->getEndPosition())
        {
          leftOffset = start - gtop->getStartPosition() ;
        }
        hal_offset_t rightOffset = 0;
        if (endPosition - 1 > gtop->getStartPosition() 
            && endPosition - 1 < gtop->getEndPosition())
        {
          rightOffset = gtop->getEndPosition() + 1 - endPosition;
        }
        // need to do offsets for edge cases
        gtIteratorToChain(gtop, chain, leftOffset, rightOffset);
        outStream << chain;
      }
      gtop->toRight();
    }
  }
  catch(hal_exception& e)
  {
    cerr << "hal exception caught: " << e.what() << endl;
    return 1;
  }
  catch(exception& e)
  {
    cerr << "Exception caught: " << e.what() << endl;
    return 1;
  }
  
  return 0;
}

