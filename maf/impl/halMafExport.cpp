/*
 * Copyright (C) 2012 by Glenn Hickey (hickey@soe.ucsc.edu)
 *
 * Released under the MIT license, see LICENSE.txt
 */

#include <deque>
#include <cassert>
#include "halMafExport.h"

using namespace std;
using namespace hal;

MafExport::MafExport() : _maxRefGap(0), _noDupes(false)
{

}

MafExport::~MafExport()
{

}

void MafExport::setMaxRefGap(hal_size_t maxRefGap)
{
  _maxRefGap = maxRefGap;
}

void MafExport::setNoDupes(bool noDupes)
{
  _noDupes = noDupes;
}

void MafExport::writeHeader()
{
  assert(_mafStream != NULL);
  if (_mafStream->tellp() == streampos(0))
  {
    *_mafStream << "##maf version=1 scoring=N/A\n"
                << "# hal " << _alignment->getNewickTree() << endl << endl;
  }
}

void MafExport::convertGenome(ostream& mafStream,
                              AlignmentConstPtr alignment,
                              const Genome* genome,
                              hal_index_t startPosition,
                              hal_size_t length,
                              const Genome* root)
{
  if (length == 0)
  {
    length = genome->getSequenceLength() - startPosition;
  }
  if ((hal_size_t)startPosition + length > genome->getSequenceLength())
  {
    throw hal_exception("Invalid range specified for convertGenome");
  }
  _mafStream = &mafStream;
  _alignment = alignment;
  writeHeader();
  hal_index_t pos = startPosition;
  hal_size_t doneLen = 0;

  while (doneLen < length)
  {
    assert(pos < (hal_index_t)genome->getSequenceLength());
    const Sequence* sequence = genome->getSequenceBySite(pos);
    assert(sequence != NULL);
    hal_index_t seqStart = pos - sequence->getStartPosition();
    hal_size_t curLen = min(length - doneLen, 
                            sequence->getSequenceLength() - seqStart);
    assert(curLen > 0);
    convertSequence(mafStream, alignment, sequence, seqStart, curLen, root);
    pos += curLen;
    doneLen += curLen;
  }
}

void MafExport::convertSequence(ostream& mafStream,
                                AlignmentConstPtr alignment,
                                const Sequence* sequence,
                                hal_index_t startPosition,
                                hal_size_t length,
                                const Genome* root)
{
  if (length == 0)
  {
    length = sequence->getSequenceLength();
  }
  if ((hal_size_t)startPosition + length > sequence->getSequenceLength())
  {
    throw hal_exception("Invalid range specified for convertSequence");
  }
  hal_index_t lastPosition = startPosition + (hal_index_t)(length - 1);
  _mafStream = &mafStream;
  _alignment = alignment;
  writeHeader();

  ColumnIteratorConstPtr colIt = sequence->getColumnIterator(root,
                                                             _maxRefGap, 
                                                             startPosition,
                                                             lastPosition,
                                                             _noDupes);
  _mafBlock.initBlock(colIt);
  assert(_mafBlock.canAppendColumn(colIt) == true);
 
  size_t numBlocks = 0;
  while (colIt->lastColumn() == false)
  {
    if (_mafBlock.canAppendColumn(colIt) == false)
    {
      // erase empty entries from the column.  helps when there are 
      // millions of sequences (ie from fastas with lots of scaffolds)
      if (numBlocks++ % 1000 == 0)
      {
        colIt->defragment();
      }
      mafStream << _mafBlock << '\n';
      _mafBlock.initBlock(colIt);
      assert(_mafBlock.canAppendColumn(colIt) == true);
    }
    _mafBlock.appendColumn(colIt);
    colIt->toRight();
  }
  mafStream << _mafBlock << endl;
}

