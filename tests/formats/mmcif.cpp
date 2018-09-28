// Chemfiles, a modern library for chemistry file reading and writing
// Copyright (C) Guillaume Fraux and contributors -- BSD license

#include <streambuf>
#include <fstream>

#include "catch.hpp"
#include "helpers.hpp"
#include "chemfiles.hpp"
using namespace chemfiles;

#include <boost/filesystem.hpp>
namespace fs=boost::filesystem;

TEST_CASE("Read files in mmCIF format") {
    SECTION("Read single step") {
        // This is how I imagine most people will resolve the conflict between
        // CIF files and mmCIF files.
        Trajectory file("data/cif/4hhb.cif", 'r', "mmCIF");
        Frame frame = file.read();

        // If comparing to the RCSB-PDB file,
        // remember that TER increases the number of atoms
        CHECK(frame.size() == 4779);

        // Check reading positions
        auto positions = frame.positions();
        CHECK(approx_eq(positions[0], Vector3D(6.204, 16.869, 4.854), 1e-3));
        CHECK(approx_eq(positions[296], Vector3D(10.167, -7.889, -16.138 ), 1e-3));
        CHECK(approx_eq(positions[4778], Vector3D(-1.263, -2.837, -21.251 ), 1e-3));

        // Check the unit cell
        auto cell = frame.cell();
        CHECK(approx_eq(cell.a(), 63.150, 1e-3));
        CHECK(approx_eq(cell.b(), 83.590, 1e-3));
        CHECK(approx_eq(cell.c(), 53.800, 1e-3));
        CHECK(approx_eq(cell.alpha(), 90.00, 1e-3));
        CHECK(approx_eq(cell.beta(), 99.34, 1e-3));
        CHECK(approx_eq(cell.gamma(), 90.00, 1e-3));

        // Check residue information
        // Note: CIF files are silly and treat all waters as one Residue....
        CHECK(frame.topology().residues().size() == 584);

        // Iron in Heme
        auto residue = *frame.topology().residue_for_atom(4557);
        CHECK(residue.size() == 43);
        CHECK(residue.name() == "HEM");
        CHECK(frame[4557].get("is_hetatm")->as_bool()); // Should be a hetatm

        // Check residue connectivity
        const auto& topo = frame.topology();
        auto residue1 = *frame.topology().residue_for_atom(0);
        // First two atoms are in the same residue
        CHECK(residue1 == *frame.topology().residue_for_atom(1));

        auto residue2 = *frame.topology().residue_for_atom(8);
        CHECK(topo.are_linked(residue1, residue2));

        auto residue3 = *frame.topology().residue_for_atom(17);
        CHECK(!topo.are_linked(residue1, residue3));
        CHECK(topo.are_linked(residue2, residue3));

        // Chain information
        CHECK(residue.get("chainid"));
        CHECK(residue.get("chainname"));

        CHECK(residue.get("chainid")->as_string() == "J");
        CHECK(residue.get("chainname")->as_string() == "D");

        CHECK(residue.contains(4525));

        // All waters for an entry are in the same residues
        auto water_res = *frame.topology().residue_for_atom(4558);
        CHECK(water_res.size() == 56);
        CHECK(water_res.name() == "HOH");

        CHECK(water_res.get("chainid"));
        CHECK(water_res.get("chainname"));

        CHECK(water_res.get("chainid")->as_string() == "K");
        CHECK(water_res.get("chainname")->as_string() == "A");
        
        // All waters for an entry are in the same residue, so this is
        // a different entity.
        auto water_res2 = *frame.topology().residue_for_atom(4614);
        CHECK(water_res2.size() == 57);
        CHECK(water_res2.name() == "HOH");

        CHECK(water_res2.get("chainid"));
        CHECK(water_res2.get("chainname"));

        CHECK(water_res2.get("chainid")->as_string() == "L");
        CHECK(water_res2.get("chainname")->as_string() == "B");
    }

    SECTION("Check nsteps") {
        Trajectory file1("data/cif/1j8k.cif", 'r', "mmCIF");
        CHECK(file1.nsteps() == 20);
    }

    SECTION("Read next step") {
        Trajectory file("data/cif/1j8k.cif", 'r', "mmCIF");
        auto frame = file.read();
        CHECK(frame.size() == 1402);

        frame = file.read();
        CHECK(frame.size() == 1402);
        auto positions = frame.positions();
        CHECK(approx_eq(positions[0], Vector3D( -9.134, 11.149, 6.990), 1e-3));
        CHECK(approx_eq(positions[1401], Vector3D(4.437, -13.250, -22.569), 1e-3));
    }

    SECTION("Read a specific step") {
        Trajectory file("data/cif/1j8k.cif", 'r', "mmCIF");

        auto frame = file.read_step(13);
        CHECK(frame.size() == 1402);
        auto positions = frame.positions();
        CHECK(approx_eq(positions[0], Vector3D(-5.106, 16.212, 4.562), 1e-3));
        CHECK(approx_eq(positions[1401], Vector3D(5.601, -22.571, -16.631), 1e-3));
        CHECK(!frame[0].get("is_hetatm")->as_bool());

        // Rewind
        frame = file.read_step(1);
        CHECK(frame.size() == 1402);
        positions = frame.positions();
        CHECK(approx_eq(positions[0], Vector3D( -9.134, 11.149, 6.990), 1e-3));
        CHECK(approx_eq(positions[1401], Vector3D(4.437, -13.250, -22.569), 1e-3));
    }

    SECTION("Read the entire file") {
        Trajectory file("data/cif/1j8k.cif", 'r', "mmCIF");
        auto frame = file.read();

        size_t count = 1;
        while (!file.done()) {
            frame = file.read();
            ++count;
        }

        CHECK(count == file.nsteps());
        CHECK(frame.size() == 1402);
    }

    SECTION("Read a COD file") {
        Trajectory file("data/cif/1544173.cif", 'r', "mmCIF");
        CHECK(file.nsteps() == 1);

        auto frame = file.read();
        CHECK(frame.size() == 50);

        auto positions = frame.positions();
        CHECK(approx_eq(positions[0], Vector3D( -0.428, 5.427, 11.536), 1e-3));
        CHECK(approx_eq(positions[1], Vector3D( -0.846, 4.873, 12.011), 1e-3));
        CHECK(approx_eq(positions[10],Vector3D(  2.507, 4.442, 8.863), 1e-3));
    }
}

TEST_CASE("Write mmCIF file") {
    auto tmpfile = NamedTempPath(".mmcif");
    const auto EXPECTED_CONTENT =
    "# generated by Chemfiles\n"
    "#\n"
    "_cell.length_a 22\n"
    "_cell.length_b 22\n"
    "_cell.length_c 22\n"
    "_cell.length_alpha 90\n"
    "_cell.length_beta  90\n"
    "_cell.length_gamma 90\n"
    "#\n"
    "loop_\n"
    "_atom_site.group_PDB\n"
    "_atom_site.id\n"
    "_atom_site.type_symbol\n"
    "_atom_site.label_atom_id\n"
    "_atom_site.label_alt_id\n"
    "_atom_site.label_comp_id\n"
    "_atom_site.label_asym_id\n"
    "_atom_site.label_seq_id\n"
    "_atom_site.Cartn_x\n"
    "_atom_site.Cartn_y\n"
    "_atom_site.Cartn_z\n"
    "_atom_site.pdbx_formal_charge\n"
    "_atom_site.auth_asym_id\n"
    "_atom_site.pdbx_PDB_model_num\n"
    "HETATM 1     A  A    .   . .    .    1.000    2.000    3.000 0 . 1\n"
    "ATOM   2     B  B    . foo ?    2    1.000    2.000    3.000 0 . 1\n"
    "ATOM   3     C  C    . foo ?    2    1.000    2.000    3.000 0 . 1\n"
    "ATOM   4     D  D    . bar G    ?    1.000    2.000    3.000 0 A 1\n"
    "HETATM 5     A  A    .   . .    .    4.000    5.000    6.000 0 . 2\n"
    "ATOM   6     B  B    . foo ?    2    1.000    2.000    3.000 0 . 2\n"
    "ATOM   7     C  C    . foo ?    2    1.000    2.000    3.000 0 . 2\n"
    "ATOM   8     D  D    . bar G    ?    1.000    2.000    3.000 0 A 2\n";


    auto frame = Frame(UnitCell(22));
    frame.add_atom(Atom("A"), {1, 2, 3});
    frame.add_atom(Atom("B"), {1, 2, 3});
    frame.add_atom(Atom("C"), {1, 2, 3});
    frame.add_atom(Atom("D"), {1, 2, 3});

    frame[0].set("is_hetatm", true);

    auto res = Residue("foo",2);
    res.add_atom(1);
    res.add_atom(2);
    frame.add_residue(std::move(res));

    res = Residue("bar");
    res.set("chainname", "A");
    res.set("chainid", "G");
    res.add_atom(3);
    frame.add_residue(std::move(res));

    auto file = Trajectory(tmpfile, 'w');
    file.write(frame);

    frame.positions()[0] = {4.0, 5.0, 6.0};
    file.write(frame);

    file.close();
    std::ifstream checking(tmpfile);
    std::string content((std::istreambuf_iterator<char>(checking)),
                         std::istreambuf_iterator<char>());
    CHECK(EXPECTED_CONTENT == content);    
}