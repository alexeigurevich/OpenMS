// --------------------------------------------------------------------------
//                   OpenMS -- Open-Source Mass Spectrometry
// --------------------------------------------------------------------------
// Copyright The OpenMS Team -- Eberhard Karls University Tuebingen,
// ETH Zurich, and Freie Universitaet Berlin 2002-2019.
//
// This software is released under a three-clause BSD license:
//  * Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//  * Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//  * Neither the name of any author or any participating institution
//    may be used to endorse or promote products derived from this software
//    without specific prior written permission.
// For a full list of authors, refer to the file AUTHORS.
// --------------------------------------------------------------------------
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL ANY OF THE AUTHORS OR THE CONTRIBUTING
// INSTITUTIONS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
// OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
// OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
// ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// --------------------------------------------------------------------------
// $Maintainer: Alexey Gurevich $
// $Authors: Alexey Gurevich $
// --------------------------------------------------------------------------

#include <OpenMS/APPLICATIONS/TOPPBase.h>
#include <OpenMS/DATASTRUCTURES/String.h>
#include <OpenMS/SYSTEM/File.h>

//#include <OpenMS/KERNEL/MSExperiment.h>
//#include <OpenMS/FORMAT/MzMLFile.h>
//#include <OpenMS/FORMAT/MzXMLFile.h>
//#include <OpenMS/FORMAT/FileHandler.h>

#include <QProcessEnvironment>
#include <QFileInfo>

#include <fstream>

using namespace OpenMS;
using namespace std; // isn't it a bad practice in C++?

//-------------------------------------------------------------
// Doxygen docu
//-------------------------------------------------------------

/**
    @page UTILS_DereplicatorAdapter DereplicatorAdapter

    @brief DereplicatorAdapter for dereplication of peptidic natural products through database search of tandem mass spectra.

<CENTER>
    <table>
        <tr>
            <td ALIGN = "center" BGCOLOR="#EBEBEB"> pot. predecessor tools </td>
            <td VALIGN="middle" ROWSPAN=2> \f$ \longrightarrow \f$ DereplicatorAdapter \f$ \longrightarrow \f$</td>
            <td ALIGN = "center" BGCOLOR="#EBEBEB"> pot. successor tools </td>
        </tr>
        <!-- What should be here? -->
    </table>
</CENTER>


    This tool can be used for dereplication of peptidic natural products (e.g. NRPs or RiPPs) through database search of MS/MS data.

    Dereplicator (a part of NPDtools) must be installed before this wrapper can be used. This wrapper was successfully tested with version v2.4.0.
    
    DereplicatorAdapter settings can be set only via command line. A param file (params.xml) is not supported yet.

    For further information check the NPDtools manual (https://github.com/ablab/npdtools/blob/master/README.md) and the official tool webpage (http://cab.spbu.ru/software/dereplicator/).

    <B>The command line parameters of this tool are:</B>
    @verbinclude UTILS_DereplicatorAdapter.cli
    <B>INI file documentation of this tool:</B>
    @htmlinclude UTILS_DereplicatorAdapter.html
*/

// We do not want this class to show up in the docu:
/// @cond TOPPCLASSES

class TOPPDereplicatorAdapter :
  public TOPPBase
{
public:
  TOPPDereplicatorAdapter() :
    TOPPBase("DereplicatorAdapter", "Dereplication of peptidic natural products through database search of mass spectra", false, 
    {
      {"Mohimani H, Gurevich A, et al",
       "Dereplication of peptidic natural products through database search of mass spectra",
       "Nature Chemical Biology 2017; 13: 30–37.",
       "10.1038/nchembio.2219"}
    })
    {}

protected:
  // this function will be used to register the tool parameters
  // it gets automatically called on tool execution
  void registerOptionsAndFlags_() override
  {
    static const bool is_required(true);
    static const bool is_advanced_option(true);

    // input, output files
    registerInputFile_("in", "<file>", "", "MzXML Input file");
    setValidFormats_("in", ListUtils::create<String>("mzXML,MGF,mzML,mzdata"));
    registerInputFile_("database", "<dir", "", "Molecular database directory. "
                                               "The directory should contain chemical structures in MOL format "
                                               "and a library.info description file.");

    registerOutputFile_("out", "<file>", "", "Output file, identification results will be saved here");
    setValidFormats_("out", ListUtils::create<String>("csv,tsv,txt"));
    
    // thirdparty executable
    registerInputFile_("executable", "<exe>", "dereplicator.py", "Python wrapper for Dereplicator. "
                       "You may skip this parameter if the wrapper is on the system PATH and executable.",
      !is_required, !is_advanced_option, ListUtils::create<String>("skipexists"));

  }

  // the main_ function is called after all parameters are read
  ExitCodes main_(int, const char **) override
  {
    //-------------------------------------------------------------
    // parsing parameters
    //-------------------------------------------------------------
    String in = getStringOption_("in");
    String database = getStringOption_("database");
    String out = getStringOption_("out");
    
    if (in.empty())  // TODO: should we also check existence here?
    {
      writeLog_("Fatal error: no input file (spectra) given");
      return ILLEGAL_PARAMETERS;
    }

    if (database.empty())
    {
      writeLog_("Fatal error: no database given");
      return ILLEGAL_PARAMETERS;
    }

    if (out.empty())
    {
      writeLog_("Fatal error: no output file (results) given");
      return ILLEGAL_PARAMETERS;
    }

    //-------------------------------------------------------------
    // determining the executable; TODO: how to get it from PATH?
    //-------------------------------------------------------------
    QString executable = getStringOption_("executable").toQString();
    if (executable.isEmpty())
    {
      writeLog_("FATAL: Executable of Dereplicator could not be found. Please either add it to PATH env variable or provide with -executable");
      return MISSING_PARAMETERS;
    }

    //Normalize file path
    QFileInfo file_info(executable);
    executable = file_info.canonicalFilePath();

    //-------------------------------------------------------------
    // process
    //-------------------------------------------------------------
    String tmp_dir = makeAutoRemoveTempDirectory_();

    QStringList arguments;
    // Check all set parameters and get them into arguments StringList
    {    
      arguments << in.toQString(); // mass spectra file
      arguments << "-o" << tmp_dir.toQString();
      arguments << "--db-path" << getStringOption_("database").toQString();
    }

    //-------------------------------------------------------------
    // run Dereplicator
    //-------------------------------------------------------------
    // Dereplicator execution with the executable (python wrapper) and the arguments StringList
    debug_level_ = 100; // to see stdout, TODO: remove later; BTW: how to enable OPENMS_LOG_DEBUG to view the full command line?
    // writeLog_("Going to execute: " + executable + " " + arguments <-- not working this way, need to iterate);
    TOPPBase::ExitCodes exit_code = runExternalProcess_(executable, arguments);
    if (exit_code != EXECUTION_OK)
    {
      return exit_code;
    }

    //-------------------------------------------------------------
    // writing output: TODO: make it nicer -- does OpenMS has function for copy/move files?
    //-------------------------------------------------------------
    // Copy results from temp directory to provided output file path
    remove(out.c_str());
    if (!out.empty())  // TODO: check file existence. Does OpenMS has function for that?
    {
      String tmp_out = tmp_dir + "significant_matches.tsv";
      std::ifstream  src(tmp_out.c_str(), std::ios::binary);
      std::ofstream  dst(out.c_str(), std::ios::binary);
      dst << src.rdbuf();
    }

    writeLog_("Everything is fine! Results are in " + out.toQString());

    return EXECUTION_OK;
  }
};


// the actual main function needed to create an executable
int main(int argc, const char ** argv)
{
  TOPPDereplicatorAdapter tool;
  return tool.main(argc, argv);
}
/// @endcond
