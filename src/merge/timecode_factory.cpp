/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes
  
   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html
  
   $Id$
  
   the timecode factory
  
   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include <map>

#include "common.h"
#include "mm_io.h"
#include "pr_generic.h"
#include "timecode_factory.h"

using namespace std;

timecode_factory_c *
timecode_factory_c::create(const string &_file_name,
                           const string &_source_name,
                           int64_t _tid) {
  mm_io_c *in;
  string line;
  int version;
  timecode_factory_c *factory;

  if (_file_name == "")
    return new timecode_factory_c("", _source_name, _tid);

  in = NULL;                    // avoid gcc warning
  try {
    in = new mm_text_io_c(new mm_file_io_c(_file_name));
  } catch(...) {
    mxerror(_("The timecode file '%s' could not be opened for reading.\n"),
            _file_name.c_str());
  }

  if (!in->getline2(line) || !starts_with_case(line, "# timecode format v") ||
      !parse_int(&line[strlen("# timecode format v")], version))
    mxerror(_("The timecode file '%s' contains an unsupported/unrecognized "
              "format line. The very first line must look like "
              "'# timecode format v1'.\n"), _file_name.c_str());
  factory = NULL;               // avoid gcc warning
  if (version == 1)
    factory = new timecode_factory_v1_c(_file_name, _source_name, _tid);
  else if (version == 2)
    factory = new timecode_factory_v2_c(_file_name, _source_name, _tid);
  else if (version == 3)
    factory = new timecode_factory_v3_c(_file_name, _source_name, _tid);
  else
    mxerror(_("The timecode file '%s' contains an unsupported/unrecognized "
              "format (version %d).\n"), _file_name.c_str(), version);

  factory->parse(*in);
  delete in;

  return factory;
}

void
timecode_factory_v1_c::parse(mm_io_c &in) {
  string line;
  timecode_range_c t;
  vector<string> fields;
  vector<timecode_range_c>::iterator iit;
  vector<timecode_range_c>::const_iterator pit;
  uint32_t i, line_no;
  bool done;

  line_no = 1;
  do {
    if (!in.getline2(line))
      mxerror(_("The timecode file '%s' does not contain a valid 'Assume' line"
                " with the default number of frames per second.\n"),
              file_name.c_str());
    line_no++;
    strip(line);
    if ((line.length() != 0) && (line[0] != '#'))
      break;
  } while (true);

  if (!starts_with_case(line, "assume "))
    mxerror(_("The timecode file '%s' does not contain a valid 'Assume' line "
              "with the default number of frames per second.\n"),
            file_name.c_str());
  line.erase(0, 6);
  strip(line);
  if (!parse_double(line.c_str(), default_fps))
    mxerror(_("The timecode file '%s' does not contain a valid 'Assume' line "
              "with the default number of frames per second.\n"),
            file_name.c_str());

  while (in.getline2(line)) {
    line_no++;
    strip(line, true);
    if ((line.length() == 0) || (line[0] == '#'))
      continue;

    if (mxsscanf(line, "%lld,%lld,%lf", &t.start_frame, &t.end_frame, &t.fps)
        != 3) {
      mxwarn(_("Line %d of the timecode file '%s' could not be parsed.\n"),
             line_no, file_name.c_str());
      continue;
    }

    if ((t.fps <= 0) || (t.start_frame < 0) || (t.end_frame < 0) ||
        (t.end_frame < t.start_frame)) {
      mxwarn(_("Line %d of the timecode file '%s' contains inconsistent data "
               "(e.g. the start frame number is bigger than the end frame "
               "number, or some values are smaller than zero).\n"),
             line_no, file_name.c_str());
      continue;
    }

    ranges.push_back(t);
  }

  mxverb(3, "ext_timecodes: Version 1, default fps %f, %u entries.\n",
         default_fps, ranges.size());

  if (ranges.size() == 0) {
    mxwarn(_("The timecode file '%s' does not contain any valid entry.\n"),
           file_name.c_str());
    t.start_frame = 0;
  } else {
    sort(ranges.begin(), ranges.end());
    do {
      done = true;
      iit = ranges.begin();
      for (i = 0; i < (ranges.size() - 1); i++) {
        iit++;
        if (ranges[i].end_frame <
            (ranges[i + 1].start_frame - 1)) {
          t.start_frame = ranges[i].end_frame + 1;
          t.end_frame = ranges[i + 1].start_frame - 1;
          t.fps = default_fps;
          ranges.insert(iit, t);
          done = false;
          break;
        }
      }
    } while (!done);
    if (ranges[0].start_frame != 0) {
      t.start_frame = 0;
      t.end_frame = ranges[0].start_frame - 1;
      t.fps = default_fps;
      ranges.insert(ranges.begin(), t);
    }
    t.start_frame = ranges[ranges.size() - 1].end_frame + 1;
  }
  t.end_frame = 0xfffffffffffffffll;
  t.fps = default_fps;
  ranges.push_back(t);

  ranges[0].base_timecode = 0.0;
  pit = ranges.begin();
  for (iit = ranges.begin() + 1; iit < ranges.end(); iit++, pit++)
    iit->base_timecode = pit->base_timecode +
      ((double)pit->end_frame - (double)pit->start_frame + 1) * 1000000000.0 /
      pit->fps;

  for (iit = ranges.begin(); iit < ranges.end(); iit++)
    mxverb(3, "ranges: entry %lld -> %lld at %f with %f\n",
           iit->start_frame, iit->end_frame, iit->fps, iit->base_timecode);
}

bool
timecode_factory_v1_c::get_next(int64_t &timecode,
                                int64_t &duration,
                                bool peek_only) {
  timecode = get_at(frameno);
  duration = get_at(frameno + 1) - timecode;
  if (!peek_only) {
    frameno++;
    if ((frameno > ranges[current_range].end_frame) &&
        (current_range < (ranges.size() - 1)))
      current_range++;
  }

  mxverb(4, "ext_timecodes v1: tc %lld dur %lld for %lld\n", timecode,
         duration, frameno - 1);
  return true;
}

int64_t
timecode_factory_v1_c::get_at(int64_t frame) {
  timecode_range_c *t;

  t = &ranges[current_range];
  if ((frame > t->end_frame) && (current_range < (ranges.size() - 1)))
    t = &ranges[current_range + 1];
  return (int64_t)(t->base_timecode + 1000000000.0 *
                   (frame - t->start_frame) / t->fps);
}

void
timecode_factory_v2_c::parse(mm_io_c &in) {
  int line_no;
  string line;
  double timecode;
  map<int64_t, int64_t> dur_map;
  map<int64_t, int64_t>::const_iterator it;
  int64_t duration, dur_sum;

  dur_sum = 0;
  line_no = 0;
  while (in.getline2(line)) {
    line_no++;
    strip(line);
    if ((line.length() == 0) || (line[0] == '#'))
      continue;
    if (!parse_double(line.c_str(), timecode))
      mxerror(_("The line %d of the timecode file '%s' does not contain a "
                "valid floating point number.\n"), line_no, file_name.c_str());
    timecodes.push_back((int64_t)(timecode * 1000000));
    if (timecodes.size() > 1) {
      duration = timecodes[timecodes.size() - 1] -
        timecodes[timecodes.size() - 2];
      if (dur_map.find(duration) == dur_map.end())
        dur_map[duration] = 1;
      else
        dur_map[duration] = dur_map[duration] + 1;
      dur_sum += duration;
      durations.push_back(duration);
    }
  }
  if (timecodes.size() == 0)
    mxerror(_("The timecode file '%s' does not contain any valid entry.\n"),
            file_name.c_str());

  dur_sum = -1;
  foreach(it, dur_map) {
    if ((dur_sum < 0) || (dur_map[dur_sum] < (*it).second))
      dur_sum = (*it).first;
    mxverb(4, "ext_timecodes v2 dur_map %lld = %lld\n", (*it).first,
           (*it).second);
  }
  mxverb(4, "ext_timecodes v2 max is %lld = %lld\n", dur_sum,
         dur_map[dur_sum]);
  if (dur_sum > 0)
    default_fps = (double)1000000000.0 / dur_sum;
  durations.push_back(dur_sum);
}

bool
timecode_factory_v2_c::get_next(int64_t &timecode,
                                int64_t &duration,
                                bool peek_only) {
  if ((frameno >= timecodes.size()) && !warning_printed) {
    mxwarn(FMT_TID "The number of external timecodes %u is "
           "smaller than the number of frames in this track. "
           "The remaining frames of this track might not be timestamped "
           "the way you intended them to be. mkvmerge might even crash.\n",
           source_name.c_str(), tid, timecodes.size());
    warning_printed = true;
    return true;
  }

  timecode = timecodes[frameno];
  duration = durations[frameno];
  if (!peek_only)
    frameno++;
  return true;
}

void
timecode_factory_v3_c::parse(mm_io_c &in) {
  string line;
  timecode_duration_c t;
  vector<string> fields;
  vector<timecode_duration_c>::iterator iit;
  vector<timecode_duration_c>::const_iterator pit;
  uint32_t line_no;
  double dur;

  line_no = 1;
  do {
    if (!in.getline2(line))
      mxerror(_("The timecode file '%s' does not contain a valid 'Assume' line"
                " with the default number of frames per second.\n"),
              file_name.c_str());
    line_no++;
    strip(line);
    if ((line.length() != 0) && (line[0] != '#'))
      break;
  } while (true);

  if (!starts_with_case(line, "assume "))
    mxerror(_("The timecode file '%s' does not contain a valid 'Assume' line "
              "with the default number of frames per second.\n"),
            file_name.c_str());
  line.erase(0, 6);
  strip(line);

  if (!parse_double(line.c_str(), default_fps))
    mxerror(_("The timecode file '%s' does not contain a valid 'Assume' line "
              "with the default number of frames per second.\n"),
            file_name.c_str());

  while (in.getline2(line)) {
    line_no++;
    strip(line, true);
    if ((line.length() == 0) || (line[0] == '#'))
      continue;

    if (starts_with_case(line, "gap,")) {
      line.erase(0, 4);
      strip(line);
      t.is_gap = true;
      t.fps = default_fps;
      if (!parse_double(line.c_str(), dur))
        mxerror(_("The timecode file '%s' does not contain a valid 'Gap' line "
                  "with the duration of the gap.\n"),
                file_name.c_str());
      t.duration = (int64_t)(1000000000.0 * dur);

    } else {
      int res;

      t.is_gap = false;
      res = mxsscanf(line, "%lf,%lf", &dur, &t.fps);
      if (res == 1) {
        t.fps = default_fps;
      } else if (res != 2) {
        mxwarn(_("Line %d of the timecode file '%s' could not be parsed.\n"),
               line_no, file_name.c_str());
        continue;
      }
      t.duration = (int64_t)(1000000000.0 * dur);
    }

    if ((t.fps < 0) || (t.duration <= 0)) {
      mxwarn(_("Line %d of the timecode file '%s' contains inconsistent data "
               "(e.g. the duration or the FPS are smaller than zero).\n"),
             line_no, file_name.c_str());
      continue;
    }

    durations.push_back(t);
  }

  mxverb(3, "ext_timecodes: Version 3, default fps %f, %u entries.\n",
         default_fps, durations.size());

  if (durations.size() == 0) {
    mxwarn(_("The timecode file '%s' does not contain any valid entry.\n"),
           file_name.c_str());
  }
  t.duration = 0xfffffffffffffffll;
  t.is_gap = false;
  t.fps = default_fps;
  durations.push_back(t);
  for (iit = durations.begin(); iit < durations.end(); iit++)
    mxverb(4, "durations:%s entry for %lld with %f FPS\n",
           iit->is_gap ? " gap" : "", iit->duration, iit->fps);
}

bool 
timecode_factory_v3_c::get_next(int64_t &timecode,
                                int64_t &duration,
                                bool peek_only) {
  bool result = (current_timecode == 0);

  if (durations[current_duration].is_gap) {
    size_t duration_index = current_duration;
    while (durations[duration_index].is_gap) {
      current_offset += durations[duration_index].duration;
      duration_index++;
    }
    if (!peek_only) {
      current_duration = duration_index;
    }
    result = true;
  }

  timecode = current_offset + current_timecode;
  // If default_fps is 0 then the duration is unchanged, usefull for audio.
  if (durations[current_duration].fps) {
    duration = (int64_t)(1000000000.0 / durations[current_duration].fps);
  }
  if (!peek_only) {
    current_timecode += duration;
    if (current_timecode >= durations[current_duration].duration) {
      current_offset += durations[current_duration].duration;
      current_timecode = 0;
      current_duration++;
    }
  }

  mxverb(3, "ext_timecodes v3: tc %lld dur %lld\n", timecode, duration);

  return result;
}
