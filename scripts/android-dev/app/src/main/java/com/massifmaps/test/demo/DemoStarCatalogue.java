package com.massifmaps.test.demo;

import java.util.LinkedHashMap;
import java.util.Map;

/**
 * The bright-star catalogue and the constellation figures the star demo draws.
 *
 * DATA, not code. Positions are J2000 right ascension (hours) and declination (degrees) with the
 * visual magnitude, for every star down to about magnitude 3 plus the fainter ones a figure needs.
 * Precession from J2000 to today is about a third of a degree and is not applied - a star is drawn
 * a few pixels across, so it does not show.
 *
 * A figure is a list of segments between star NAMES: a typo is then a missing line and a log
 * warning rather than a silently wrong sky.
 */
public final class DemoStarCatalogue {

    private DemoStarCatalogue() {
    }

    /** name | right ascension (hours) | declination (degrees) | visual magnitude */
    public static final String[] STARS = {
        // --- brightest, all sky -----------------------------------------------------------------
        "Sirius|6.7525|-16.7161|-1.46",
        "Canopus|6.3992|-52.6957|-0.72",
        "Rigil Kentaurus|14.6600|-60.8339|-0.27",
        "Arcturus|14.2610|19.1825|-0.05",
        "Vega|18.6156|38.7837|0.03",
        "Capella|5.2782|45.9980|0.08",
        "Rigel|5.2423|-8.2016|0.13",
        "Procyon|7.6551|5.2250|0.34",
        "Achernar|1.6286|-57.2367|0.46",
        "Betelgeuse|5.9195|7.4071|0.50",
        "Hadar|14.0637|-60.3730|0.61",
        "Altair|19.8464|8.8683|0.77",
        "Acrux|12.4433|-63.0991|0.77",
        "Aldebaran|4.5987|16.5093|0.85",
        "Spica|13.4199|-11.1613|1.04",
        "Antares|16.4901|-26.4320|1.09",
        "Pollux|7.7553|28.0262|1.14",
        "Fomalhaut|22.9608|-29.6222|1.16",
        "Deneb|20.6905|45.2803|1.25",
        "Mimosa|12.7952|-59.6888|1.25",
        "Regulus|10.1395|11.9672|1.35",
        "Adhara|6.9771|-28.9720|1.50",
        "Castor|7.5766|31.8883|1.58",
        "Shaula|17.5601|-37.1038|1.62",
        "Gacrux|12.5194|-57.1133|1.63",
        "Bellatrix|5.4185|6.3497|1.64",
        "Elnath|5.4381|28.6075|1.65",
        "Miaplacidus|9.2200|-69.7172|1.67",
        "Alnilam|5.6036|-1.2019|1.69",
        "Alnair|22.1372|-46.9611|1.74",
        "Alnitak|5.6793|-1.9426|1.77",
        "Alioth|12.9005|55.9598|1.77",
        "Dubhe|11.0622|61.7510|1.79",
        "Mirfak|3.4054|49.8612|1.79",
        "Wezen|7.1399|-26.3932|1.83",
        "Regor|8.1589|-47.3367|1.83",
        "Kaus Australis|18.4029|-34.3846|1.85",
        "Alkaid|13.7923|49.3133|1.86",
        "Sargas|17.6220|-42.9978|1.86",
        "Avior|8.3752|-59.5095|1.86",
        "Menkalinan|5.9922|44.9474|1.90",
        "Atria|16.8111|-69.0277|1.91",
        "Alhena|6.6285|16.3993|1.93",
        "Peacock|20.4275|-56.7351|1.94",
        "Alsephina|8.7451|-54.7085|1.96",
        "Mirzam|6.3783|-17.9559|1.98",
        "Polaris|2.5303|89.2641|1.98",
        "Alphard|9.4597|-8.6586|1.99",
        "Hamal|2.1195|23.4624|2.00",
        "Deneb Kaitos|0.7265|-17.9866|2.04",
        "Nunki|18.9211|-26.2967|2.05",
        "Mirach|1.1622|35.6206|2.05",
        "Alpheratz|0.1398|29.0904|2.06",
        "Menkent|14.1114|-36.3697|2.06",
        "Saiph|5.7959|-9.6697|2.09",
        "Kochab|14.8451|74.1555|2.08",
        "Rasalhague|17.5822|12.5600|2.08",
        "Algieba|10.3329|19.8415|2.08",
        "Algol|3.1361|40.9556|2.09",
        "Almach|2.0650|42.3297|2.10",
        "Denebola|11.8177|14.5720|2.14",
        "Muhlifain|12.6919|-48.9597|2.20",
        "Naos|8.0597|-40.0033|2.21",
        "Aspidiske|9.2848|-59.2753|2.21",
        "Alphecca|15.5781|26.7147|2.22",
        "Suhail|9.1333|-43.4326|2.23",
        "Sadr|20.3705|40.2567|2.23",
        "Mizar|13.3987|54.9254|2.23",
        "Eltanin|17.9435|51.4889|2.23",
        "Schedar|0.6751|56.5375|2.24",
        "Mintaka|5.5334|-0.2991|2.25",
        "Caph|0.1530|59.1498|2.28",
        "Larawag|16.8361|-34.2933|2.29",
        "Dschubba|16.0056|-22.6217|2.29",
        "Epsilon Centauri|13.6648|-53.4664|2.30",
        "Eta Centauri|14.5918|-42.1578|2.31",
        "Kakkab|14.6988|-47.3881|2.30",
        "Merak|11.0307|56.3824|2.37",
        "Izar|14.7498|27.0742|2.37",
        "Enif|21.7364|9.8750|2.39",
        "Ankaa|0.4381|-42.3061|2.40",
        "Girtab|17.7081|-39.0300|2.41",
        "Scheat|23.0629|28.0828|2.42",
        "Sabik|17.1730|-15.7250|2.43",
        "Phecda|11.8972|53.6948|2.44",
        "Alderamin|21.3096|62.5856|2.45",
        "Aludra|7.4016|-29.3031|2.45",
        "Navi|0.9451|60.7167|2.47",
        "Markeb|9.3685|-55.0108|2.47",
        "Aljanah|20.7702|33.9703|2.48",
        "Markab|23.0793|15.2053|2.49",
        "Menkar|3.0380|4.0897|2.53",
        "Zosma|11.2351|20.5237|2.56",
        "Zeta Ophiuchi|16.6193|-10.5672|2.56",
        "Acrab|16.0906|-19.8056|2.56",
        "Arneb|5.5455|-17.8222|2.58",
        "Ascella|19.0435|-29.8803|2.60",
        "Zubeneschamali|15.2834|-9.3829|2.61",
        "Theta Aurigae|5.9954|37.2125|2.62",
        "Unukalhai|15.7378|6.4256|2.63",
        "Sheratan|1.9107|20.8081|2.64",
        "Muphrid|13.9114|18.3978|2.68",
        "Hassaleh|4.9499|33.1661|2.69",
        "Lesath|17.5127|-37.2958|2.69",
        "Delta Crucis|12.2524|-58.7489|2.79",
        "Tarazed|19.7710|10.6133|2.72",
        "Yed Prior|16.2391|-3.6942|2.73",
        "Porrima|12.6944|-1.4494|2.74",
        "Zubenelgenubi|14.8480|-16.0417|2.75",
        "Cebalrai|17.7246|4.5672|2.76",
        "Hatysa|5.5906|-5.9100|2.77",
        "Rastaban|17.5072|52.3014|2.79",
        "Nihal|5.4707|-20.7594|2.81",
        "Kaus Borealis|18.4662|-25.4217|2.81",
        "Tau Scorpii|16.5981|-28.2161|2.82",
        "Vindemiatrix|13.0363|10.9592|2.83",
        "Algenib|0.2206|15.1836|2.83",
        "Zeta Persei|3.9022|31.8836|2.85",
        "Tejat|6.3827|22.5136|2.87",
        "Alcyone|3.7914|24.1053|2.87",
        "Delta Cygni|19.7496|45.1308|2.87",
        "Pi Scorpii|15.9809|-26.1142|2.89",
        "Gomeisa|7.4525|8.2894|2.89",
        "Epsilon Persei|3.9642|40.0103|2.90",
        "Gamma Persei|3.0799|53.5064|2.93",
        "Matar|22.7167|30.2214|2.94",
        "Mebsuta|6.7322|25.1311|2.98",
        "Epsilon Leonis|9.7642|23.7742|2.98",
        "Alnasl|18.0968|-30.4242|2.99",
        "Zeta Aquilae|19.0902|13.8633|2.99",
        "Mu Scorpii|16.8644|-38.0475|3.00",
        "Zeta Tauri|5.6274|21.1425|3.00",
        "Gamma Hydrae|13.3153|-23.1714|3.00",
        "Delta Persei|3.7154|47.7875|3.01",
        "Epsilon Aurigae|5.0328|43.8233|3.03",
        "Seginus|14.5346|38.3083|3.03",
        "Iota Scorpii|17.7931|-40.1269|3.03",
        "Pherkad|15.3455|71.8339|3.05",
        "Albireo|19.5121|27.9597|3.05",
        "Delta Draconis|19.2093|67.6617|3.07",
        "Ruchbah|1.4304|60.2353|2.68",
        "Zeta Draconis|17.1465|65.7147|3.17",
        "Phi Sagittarii|18.7609|-26.9908|3.17",
        "Pi3 Orionis|4.8307|6.9614|3.19",
        "Errai|23.6558|77.6325|3.21",
        "Alfirk|21.4777|70.5608|3.23",
        "Sulafat|18.9824|32.6894|3.24",
        "Yed Posterior|16.3054|-4.6925|3.24",
        "Kaus Media|18.3499|-29.8281|2.70",
        "Theta Aquilae|20.1884|-0.8214|3.24",
        "Eta Geminorum|6.2480|22.5067|3.28",
        "Sigma Librae|15.0678|-25.2819|3.29",
        "Megrez|12.2571|57.0326|3.31",
        "Chertan|11.2373|15.4297|3.32",
        "Tau Sagittarii|19.1157|-27.6706|3.32",
        "Delta Aquilae|19.4250|3.1147|3.36",
        "Xi Geminorum|6.7548|12.8956|3.36",
        "Epsilon Cassiopeiae|1.9066|63.6700|3.37",
        "Zeta Virginis|13.5782|-0.5958|3.38",
        "Delta Virginis|12.9267|3.3975|3.38",
        "Meissa|5.5856|9.9342|3.39",
        "Theta2 Tauri|4.4777|15.8708|3.40",
        "Homam|22.6910|10.8314|3.40",
        "Adhafera|10.2782|23.4172|3.44",
        "Delta Bootis|15.2584|33.3147|3.47",
        "Gamma Ceti|2.7217|3.2358|3.47",
        "Nekkar|15.0324|40.3906|3.49",
        "Eta Leonis|10.1222|16.7628|3.51",
        "Sheliak|18.8347|33.3628|3.52",
        "Wasat|7.3354|21.9822|3.53",
        "Ain|4.4769|19.1806|3.53",
        "Hyadum I|4.3299|15.6278|3.65",
        "Thuban|14.0732|64.3758|3.65",
        "Nusakan|15.4638|29.1058|3.66",
        "Alshain|19.9219|6.4067|3.71",
        "Delta Tauri|4.3822|17.5425|3.76",
        "Mekbuda|7.0685|20.5703|3.79",
        "Delta Andromedae|0.6555|30.8611|3.27",
        "Rasalas|9.8794|26.0069|3.88",
        "Epsilon Ursae Minoris|16.7661|82.0372|4.21",
        "Zeta Ursae Minoris|15.7343|77.7944|4.32",
        "Delta Ursae Minoris|17.5369|86.5864|4.36",
        "Eta Ursae Minoris|16.2918|75.7553|4.95",
        "Pi Puppis|7.2857|-37.0975|2.71",
    };

    /**
     * The figures, as segments between star names. These are the common stick figures, kept to the
     * stars above - a few well known ones (Ursa Major's plough, Orion, the Southern Cross) rather
     * than all 88 boundaries, which is what makes a sky readable at a glance.
     */
    public static Map<String, String[][]> figures() {
        Map<String, String[][]> figures = new LinkedHashMap<String, String[][]>();
        figures.put("Orion", segments(
            "Betelgeuse", "Bellatrix", "Bellatrix", "Mintaka", "Mintaka", "Alnilam", "Alnilam", "Alnitak",
            "Alnitak", "Betelgeuse", "Mintaka", "Rigel", "Alnitak", "Saiph", "Rigel", "Saiph",
            "Betelgeuse", "Meissa", "Meissa", "Bellatrix", "Alnilam", "Hatysa"));
        figures.put("Ursa Major", segments(
            "Alkaid", "Mizar", "Mizar", "Alioth", "Alioth", "Megrez", "Megrez", "Phecda",
            "Phecda", "Merak", "Merak", "Dubhe", "Dubhe", "Megrez"));
        figures.put("Ursa Minor", segments(
            "Polaris", "Delta Ursae Minoris", "Delta Ursae Minoris", "Epsilon Ursae Minoris",
            "Epsilon Ursae Minoris", "Zeta Ursae Minoris", "Zeta Ursae Minoris", "Kochab",
            "Kochab", "Pherkad", "Pherkad", "Eta Ursae Minoris", "Eta Ursae Minoris", "Zeta Ursae Minoris"));
        figures.put("Cassiopeia", segments(
            "Caph", "Schedar", "Schedar", "Navi", "Navi", "Ruchbah", "Ruchbah", "Epsilon Cassiopeiae"));
        figures.put("Cygnus", segments(
            "Deneb", "Sadr", "Sadr", "Albireo", "Aljanah", "Sadr", "Sadr", "Delta Cygni"));
        figures.put("Lyra", segments(
            "Vega", "Sheliak", "Sheliak", "Sulafat", "Sulafat", "Vega"));
        figures.put("Aquila", segments(
            "Tarazed", "Altair", "Altair", "Alshain", "Tarazed", "Zeta Aquilae",
            "Altair", "Delta Aquilae", "Delta Aquilae", "Theta Aquilae"));
        figures.put("Scorpius", segments(
            "Dschubba", "Acrab", "Dschubba", "Pi Scorpii", "Dschubba", "Antares", "Antares", "Tau Scorpii",
            "Tau Scorpii", "Larawag", "Larawag", "Mu Scorpii", "Mu Scorpii", "Sargas", "Sargas", "Iota Scorpii",
            "Iota Scorpii", "Girtab", "Girtab", "Shaula", "Shaula", "Lesath"));
        figures.put("Sagittarius", segments(
            "Alnasl", "Kaus Media", "Kaus Australis", "Kaus Media", "Kaus Media", "Kaus Borealis",
            "Kaus Borealis", "Nunki", "Nunki", "Ascella", "Ascella", "Kaus Australis",
            "Nunki", "Tau Sagittarii", "Tau Sagittarii", "Phi Sagittarii", "Phi Sagittarii", "Kaus Borealis"));
        figures.put("Leo", segments(
            "Regulus", "Eta Leonis", "Eta Leonis", "Algieba", "Algieba", "Adhafera", "Adhafera", "Rasalas",
            "Rasalas", "Epsilon Leonis", "Algieba", "Zosma", "Zosma", "Denebola", "Denebola", "Chertan",
            "Chertan", "Regulus"));
        figures.put("Taurus", segments(
            "Elnath", "Ain", "Ain", "Aldebaran", "Aldebaran", "Theta2 Tauri", "Theta2 Tauri", "Hyadum I",
            "Aldebaran", "Delta Tauri", "Zeta Tauri", "Aldebaran"));
        figures.put("Gemini", segments(
            "Castor", "Pollux", "Castor", "Mebsuta", "Mebsuta", "Tejat", "Tejat", "Eta Geminorum",
            "Pollux", "Wasat", "Wasat", "Mekbuda", "Mekbuda", "Alhena", "Wasat", "Xi Geminorum"));
        figures.put("Canis Major", segments(
            "Mirzam", "Sirius", "Sirius", "Wezen", "Wezen", "Adhara", "Adhara", "Mirzam",
            "Wezen", "Aludra"));
        figures.put("Bootes", segments(
            "Arcturus", "Izar", "Izar", "Seginus", "Seginus", "Nekkar", "Nekkar", "Delta Bootis",
            "Delta Bootis", "Izar", "Arcturus", "Muphrid"));
        figures.put("Crux", segments(
            "Acrux", "Gacrux", "Mimosa", "Delta Crucis"));
        figures.put("Centaurus", segments(
            "Rigil Kentaurus", "Hadar", "Hadar", "Epsilon Centauri", "Epsilon Centauri", "Muhlifain",
            "Muhlifain", "Menkent", "Menkent", "Eta Centauri", "Eta Centauri", "Hadar"));
        figures.put("Pegasus", segments(
            "Markab", "Scheat", "Scheat", "Alpheratz", "Alpheratz", "Algenib", "Algenib", "Markab",
            "Markab", "Homam", "Scheat", "Matar", "Homam", "Enif"));
        figures.put("Andromeda", segments(
            "Alpheratz", "Delta Andromedae", "Delta Andromedae", "Mirach", "Mirach", "Almach"));
        figures.put("Auriga", segments(
            "Capella", "Menkalinan", "Menkalinan", "Theta Aurigae", "Theta Aurigae", "Elnath",
            "Elnath", "Hassaleh", "Hassaleh", "Epsilon Aurigae", "Epsilon Aurigae", "Capella"));
        figures.put("Perseus", segments(
            "Mirfak", "Algol", "Algol", "Zeta Persei", "Mirfak", "Delta Persei", "Delta Persei", "Epsilon Persei",
            "Mirfak", "Gamma Persei"));
        figures.put("Virgo", segments(
            "Spica", "Zeta Virginis", "Zeta Virginis", "Porrima", "Porrima", "Delta Virginis",
            "Delta Virginis", "Vindemiatrix"));
        figures.put("Aries", segments(
            "Hamal", "Sheratan"));
        figures.put("Cepheus", segments(
            "Alderamin", "Alfirk", "Alfirk", "Errai", "Errai", "Alderamin"));
        figures.put("Draco", segments(
            "Eltanin", "Rastaban", "Rastaban", "Zeta Draconis", "Zeta Draconis", "Delta Draconis",
            "Delta Draconis", "Thuban"));
        figures.put("Corona Borealis", segments(
            "Alphecca", "Nusakan"));
        figures.put("Canis Minor", segments(
            "Procyon", "Gomeisa"));
        figures.put("Ophiuchus", segments(
            "Rasalhague", "Cebalrai", "Cebalrai", "Sabik", "Sabik", "Zeta Ophiuchi",
            "Zeta Ophiuchi", "Yed Prior", "Yed Prior", "Yed Posterior", "Yed Prior", "Rasalhague"));
        figures.put("Libra", segments(
            "Zubeneschamali", "Zubenelgenubi", "Zubenelgenubi", "Sigma Librae", "Sigma Librae", "Zubeneschamali"));
        figures.put("Carina", segments(
            "Canopus", "Avior", "Avior", "Aspidiske", "Aspidiske", "Miaplacidus"));
        figures.put("Vela", segments(
            "Regor", "Markeb", "Markeb", "Alsephina", "Alsephina", "Suhail", "Suhail", "Regor"));
        return figures;
    }

    /** Pairs of names, in order, into a segment list. */
    private static String[][] segments(String... names) {
        String[][] result = new String[names.length / 2][2];
        for (int i = 0; i + 1 < names.length; i += 2) {
            result[i / 2][0] = names[i];
            result[i / 2][1] = names[i + 1];
        }
        return result;
    }
}
