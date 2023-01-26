#pragma once

/* gplot++
 *
 * A header-only C++ interface to Gnuplot.
 *
 * Copyright 2020 Maurizio Tomasi
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Version history
 *
 * - 0.5.0 (2021/12/02): use a smarter algorithm to specify ranges
 *                       add `redirect_to_dumb` (and TerminalType enum)
 *
 * - 0.4.0 (2021/11/04): 2D/3D vector plots, SVG saving
 *
 * - 0.3.1 (2021/10/23): error bars
 *
 * - 0.3.0 (2021/10/23): support for Windows
 *
 * - 0.2.1 (2020/12/16): ensure that commands sent to Gnuplot are executed
 *   immediately
 *
 * - 0.2.0 (2020/12/14): 3D plots
 *
 * - 0.1.0 (2020/11/29): first release
 *
 */

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <Windows.h>
#else
#include <unistd.h>
#endif

const unsigned GNUPLOTPP_VERSION = 0x000500;
const unsigned GNUPLOTPP_MAJOR_VERSION = (GNUPLOTPP_VERSION & 0xFF0000) >> 16;
const unsigned GNUPLOTPP_MINOR_VERSION = (GNUPLOTPP_VERSION & 0x00FF00) >> 8;
const unsigned GNUPLOTPP_PATCH_VERSION = (GNUPLOTPP_VERSION & 0xFF);

/**
 * High-level interface to the Gnuplot executable
 *
 * This class establishes a connection with a new Gnuplot instance and
 * sends commands to it through a pipe. It is able to directly plot
 * vectors of numbers by saving them in temporary files.
 */
class Gnuplot {
private:
  static FILE *safe_popen(const char *name, const char *mode) {
#ifdef _WIN32
    return _popen(name, mode);
#else
    return popen(name, mode);
#endif
  }

  static int safe_pclose(FILE *f) {
#ifdef _WIN32
    return _pclose(f);
#else
    return pclose(f);
#endif
  }

  static void safe_sleep(unsigned seconds) {
#ifdef _WIN32
    Sleep(seconds * 1000); // Sleep on Windows requires milliseconds
#else
    sleep(seconds);
#endif
  }

  std::string escape_quotes(const std::string &s) {
    std::string result{};

    for (char c : s) {
      if (c != '\'')
        result.push_back(c);
      else
        result += "''";
    }

    return result;
  }

public:
  enum class LineStyle {
    DOTS,
    LINES,
    POINTS,
    LINESPOINTS,
    STEPS,
    BOXES,
    X_ERROR_BARS,
    Y_ERROR_BARS,
    XY_ERROR_BARS,
    VECTORS,
  };

  enum class AxisScale {
    LINEAR,
    LOGX,
    LOGY,
    LOGXY,
  };

  enum class TerminalMode {
	MONO,
	ANSI,
	ANSI256,
	ANSIRGB,
  };

  Gnuplot(const char *executable_name = "gnuplot", bool persist = true)
      : connection{}, series{}, files_to_delete{}, is_3dplot{false} {
    std::stringstream os;
    // The --persist flag lets Gnuplot keep running after the C++
    // program has completed its execution
    os << executable_name;
    if (persist)
      os << " --persist";
    connection = safe_popen(os.str().c_str(), "w");

    set_xrange();
    set_yrange();
    set_zrange();

    // See
    // https://stackoverflow.com/questions/28152719/how-to-make-gnuplot-use-the-unicode-minus-sign-for-negative-numbers
    sendcommand("set encoding utf8\n");
    sendcommand("set minussign");
  }

  ~Gnuplot() {
    // Bye bye, Gnuplot!
    if (connection) {
      safe_pclose(connection);
      connection = nullptr;
    }

    // Let some time pass before removing the files, so that Gnuplot
    // can finish displaying the last plot.
    safe_sleep(1);

    // Now remove the data files
    for (const auto &fname : files_to_delete) {
      std::remove(fname.c_str());
    }
  }

  /* This is the most low-level method in the Gnuplot class! It
         returns `true` if the send command was successful, `false`
         otherwise. */
  bool sendcommand(const char *str) {
    if (!ok())
      return false;

    fputs(str, connection);
    fputc('\n', connection);
    fflush(connection);

    return true;
  }

  bool sendcommand(const std::string &str) { return sendcommand(str.c_str()); }
  bool sendcommand(const std::stringstream &stream) {
    return sendcommand(stream.str());
  }

  bool ok() { return connection != nullptr; }

  /* Save the plot to a PNG file instead of displaying a window */
  bool redirect_to_png(const std::string &filename,
                       const std::string &size = "800,600") {
    std::stringstream os;

    os << "set terminal pngcairo color enhanced size " << size << "\n"
       << "set output '" << filename << "'\n";
    return sendcommand(os);
  }

  /* Save the plot to a PDF file instead of displaying a window */
  bool redirect_to_pdf(const std::string &filename,
                       std::string size = "16cm,12cm") {
    std::stringstream os;

    os << "set terminal pdfcairo color enhanced size " << size << "\n"
       << "set output '" << filename << "'\n";
    return sendcommand(os);
  }

  /* Save the plot to a SVG file instead of displaying a window */
  bool redirect_to_svg(const std::string &filename,
                       const std::string &size = "800,600") {
    std::stringstream os;

    os << "set terminal svg enhanced mouse standalone size " << size << "\n"
       << "set output '" << filename << "'\n";
    return sendcommand(os);
  }

  /* Send the plot to the terminal or to a text file */
  bool redirect_to_dumb(const std::string &filename = "",
                        unsigned int width = 80,
						unsigned int height = 50,
						TerminalMode mode = TerminalMode::MONO) {
    std::stringstream os;

	os << "set terminal dumb size " << width << " " << height;

	switch(mode) {
	case TerminalMode::MONO: os << "mono"; break;
	case TerminalMode::ANSI: os << "ansi"; break;
	case TerminalMode::ANSI256: os << "ansi256"; break;
	case TerminalMode::ANSIRGB: os << "ansirgb"; break;
	default: os << "mono";
	}
	
	os << "\n";

	if (! filename.empty()) {
	  os << "set output '" << filename << "'\n";
	}
	
    return sendcommand(os);
  }

  /* Set the label on the X axis */
  bool set_xlabel(const std::string &label) {
    std::stringstream os;
    os << "set xlabel '" << escape_quotes(label) << "'";
    return sendcommand(os);
  }

  /* Set the label on the Y axis */
  bool set_ylabel(const std::string &label) {
    std::stringstream os;
    os << "set ylabel '" << escape_quotes(label) << "'";
    return sendcommand(os);
  }

  /* Set the minimum and maximum value to be displayed along the X axis */
  void set_xrange(double min = NAN, double max = NAN) {
    xrange = format_range(min, max);
  }

  /* Set the minimum and maximum value to be displayed along the X axis */
  void set_yrange(double min = NAN, double max = NAN) {
    yrange = format_range(min, max);
  }

  /* Set the minimum and maximum value to be displayed along the X axis */
  void set_zrange(double min = NAN, double max = NAN) {
    zrange = format_range(min, max);
  }

  bool set_logscale(AxisScale scale) {
    switch (scale) {
    case AxisScale::LOGX:
      return sendcommand("set logscale x");
    case AxisScale::LOGY:
      return sendcommand("set logscale y");
    case AxisScale::LOGXY:
      return sendcommand("set logscale xy");
    default:
      return sendcommand("unset logscale");
    }
  }

  template <typename T>
  void plot(const std::vector<T> &y, const std::string &label = "",
            LineStyle style = LineStyle::LINES) {
    _plot(label, style, false, y);
  }

  template <typename T, typename U>
  void plot(const std::vector<T> &x, const std::vector<U> &y,
            const std::string &label = "", LineStyle style = LineStyle::LINES) {
    _plot(label, style, false, x, y);
  }

  template <typename T, typename U, typename V>
  void plot_xerr(const std::vector<T> &x, const std::vector<U> &y,
                 const std::vector<V> &err, const std::string &label = "") {
    _plot(label, LineStyle::X_ERROR_BARS, false, x, y, err);
  }

  template <typename T, typename U, typename V>
  void plot_yerr(const std::vector<T> &x, const std::vector<U> &y,
                 const std::vector<V> &err, const std::string &label = "") {
    _plot(label, LineStyle::Y_ERROR_BARS, false, x, y, err);
  }

  template <typename T, typename U, typename V, typename W>
  void plot_xyerr(const std::vector<T> &x, const std::vector<U> &y,
                  const std::vector<V> &xerr, const std::vector<W> &yerr,
                  const std::string &label = "") {
    _plot(label, LineStyle::XY_ERROR_BARS, false, x, y, xerr, yerr);
  }

  template <typename T, typename U, typename V, typename W>
  void plot_vectors(const std::vector<T> &x, const std::vector<U> &y,
                    const std::vector<V> &vx, const std::vector<W> &vy,
                    const std::string &label = "") {
    _plot(label, LineStyle::VECTORS, false, x, y, vx, vy);
  }

  template <typename T, typename U>
  void plot3d(const std::vector<T> &x, const std::vector<U> &y,
              const std::vector<U> &z, const std::string &label = "",
              LineStyle style = LineStyle::LINES) {
    _plot(label, style, true, x, y, z);
  }

  template <typename T, typename U, typename V, typename W>
  void plot_vectors3d(const std::vector<T> &x, const std::vector<U> &y,
                      const std::vector<T> &z, const std::vector<V> &vx,
                      const std::vector<W> &vy, const std::vector<T> &vz,
                      const std::string &label = "") {
    _plot(label, LineStyle::VECTORS, true, x, y, z, vx, vy, vz);
  }

  template <typename T>
  void histogram(const std::vector<T> &values, size_t nbins,
                 const std::string &label = "",
                 LineStyle style = LineStyle::BOXES) {
    assert(nbins > 0);

    if (values.empty())
      return;

    if (!series.empty()) {
      assert(!is_3dplot);
    }

    double min = *std::min_element(values.begin(), values.end());
    double max = *std::max_element(values.begin(), values.end());
    double binwidth = (max - min) / nbins;

    std::vector<size_t> bins(nbins);
    for (const auto &val : values) {
      int index = static_cast<int>((val - min) / binwidth);
      if (index >= int(nbins))
        --index;

      bins.at(index)++;
    }

    std::stringstream of;
    for (size_t i{}; i < nbins; ++i) {
      of << min + binwidth * (i + 0.5) << " " << bins[i] << "\n";
    }

    series.push_back(GnuplotSeries{of.str(), style, label, "1:2"});
    is_3dplot = false;
  }

  // Ask Gnuplot to use a multiple-plot layout
  bool multiplot(int nrows, int ncols, const std::string &title = "") {
    std::stringstream os;
    os << "set multiplot layout " << nrows << ", " << ncols << " title '"
       << escape_quotes(title) << "'\n";
    return sendcommand(os);
  }

  // Force Gnuplot to draw all the series sent through any of the `plot`
  // commands
  bool show(bool call_reset = true) {
    if (series.empty())
      return true;

    std::stringstream os;
    os << "set style fill solid 0.5\n";

    // Write the data in separate series
    for (size_t i{}; i < series.size(); ++i) {
      const GnuplotSeries &s = series.at(i);
      os << "$Datablock" << i << " << EOD\n" << s.data_string << "\nEOD\n";
    }

    if (is_3dplot) {
      os << "splot " << xrange << " " << yrange << " " << zrange << " ";
    } else {
      os << "plot " << xrange << " " << yrange << " ";
    }

    // Plot the series we have just defined
    for (size_t i{}; i < series.size(); ++i) {
      const GnuplotSeries &s = series.at(i);
      os << "$Datablock" << i << " using " << s.column_range << " with "
         << style_to_str(s.line_style) << " title '" << escape_quotes(s.title)
         << "'";

      if (i + 1 < series.size())
        os << ", ";
    }

    bool result = sendcommand(os);
    if (result && call_reset)
      reset();

    return result;
  }

  // Remove all the series from memory and start with a blank plot
  void reset() {
    series.clear();
    set_xrange();
    set_yrange();
    is_3dplot = false;
  }

private:
  void _print_ith_elements(std::ostream &, std::ostream &, int, size_t) {}

  template <typename T, typename... Args>
  void _print_ith_elements(std::ostream &os, std::ostream &fmts, int index,
                           size_t i, const std::vector<T> &v, Args... args) {
    os << v[i] << " ";

    if (i == 0) {
      if (index > 1)
        fmts << ':';
      fmts << index;
    }

    _print_ith_elements(os, fmts, index + 1, i, args...);
  }

  template <typename T, typename... Args>
  void _plot(const std::string &label, LineStyle style, bool is_this_3dplot,
             const std::vector<T> &v, Args... args) {
    if (v.empty())
      return;

    if (!series.empty()) {
      // Check that we are not adding a 3D plot to a 2D plot, or vice versa
      assert(is_3dplot == is_this_3dplot);
    }

    std::stringstream of;
    std::stringstream fmtstring;
    for (size_t i{}; i < v.size(); ++i) {
      _print_ith_elements(of, fmtstring, 1, i, v, args...);
      of << "\n";
    }

    series.push_back(GnuplotSeries{of.str(), style, label, fmtstring.str()});
    is_3dplot = is_this_3dplot;
  }

  struct GnuplotSeries {
    std::string data_string;
    LineStyle line_style;
    std::string title;
    std::string column_range;
  };

  std::string style_to_str(LineStyle style) {
    switch (style) {
    case LineStyle::DOTS:
      return "dots";
    case LineStyle::POINTS:
      return "points";
    case LineStyle::LINESPOINTS:
      return "linespoints";
    case LineStyle::STEPS:
      return "steps";
    case LineStyle::BOXES:
      return "boxes";
    case LineStyle::X_ERROR_BARS:
      return "xerrorbars";
    case LineStyle::Y_ERROR_BARS:
      return "yerrorbars";
    case LineStyle::XY_ERROR_BARS:
      return "xyerrorbars";
    case LineStyle::VECTORS:
      return "vectors";
    default:
      return "lines";
    }
  }

  std::string format_range(double min = NAN, double max = NAN) {
    if (std::isnan(min) && std::isnan(max))
      return "[]";

    std::stringstream os;
    os << "[";

    if (std::isnan(min))
      os << "*";
    else
      os << min;

    os << ":";
    if (std::isnan(max))
      os << "*";
    else
      os << max;

    os << "]";

    return os.str();
  }

  FILE *connection;
  std::vector<GnuplotSeries> series;
  std::vector<std::string> files_to_delete;
  std::string xrange;
  std::string yrange;
  std::string zrange;
  bool is_3dplot;
};
