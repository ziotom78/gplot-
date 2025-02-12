/* Copyright 2020 Maurizio Tomasi
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
 */

#include "gplot++.h"
#include <cstdio>
#include <iostream>

int main(void) {
  Gnuplot gnuplot{};
  std::vector<double> x{1, 2, 3, 4, 5}, y{5, 2, 4, 1, 3};

  std::cout << "Running gplot++ v" << GNUPLOTPP_MAJOR_VERSION << "."
            << GNUPLOTPP_MINOR_VERSION << "." << GNUPLOTPP_PATCH_VERSION
            << "\n";

  gnuplot.redirect_to_animated_gif("animation.gif", "800,600", 1000, true);

  for(int i{}; i < (int) x.size(); ++i) {
	gnuplot.add_point(x[i], y[i]);
	gnuplot.plot(); // Do the plot

	// In an animation, it is advisable to force the x/y ranges in advance
	gnuplot.set_xrange(0, 6);
	gnuplot.set_yrange(0, 6);

	gnuplot.show(); // Add the frame to the GIF file
  }

  gnuplot.show();

  std::cout << "Press any key to quit...";
  std::getchar();
}
