open Grain_parsing;

type node_t =
  | Code(Grain_parsing.Location.t)
  | Comment((Grain_parsing.Location.t, Grain_parsing.Parsetree.comment));

let allLocations: ref(list(node_t)) = ref([]);

let getNodeLoc = (node: node_t): Grain_parsing.Location.t =>
  switch (node) {
  | Code(loc) => loc
  | Comment((loc, _)) => loc
  };

let getCommentLoc = (comment: Parsetree.comment) =>
  switch (comment) {
  | Line(cmt) => cmt.cmt_loc
  | Block(cmt) => cmt.cmt_loc
  | Doc(cmt) => cmt.cmt_loc
  | Shebang(cmt) => cmt.cmt_loc
  };

// iterate the tree to make an ordered list of locations
// we can then find expressions between locations

//

let get_raw_pos_info = (pos: Lexing.position) => (
  pos.pos_fname,
  pos.pos_lnum,
  pos.pos_cnum - pos.pos_bol,
  pos.pos_bol,
);

let print_loc = (msg: string, loc: Grain_parsing.Location.t) => {
  let (file, line, startchar, _) = get_raw_pos_info(loc.loc_start);
  let (_, endline, endchar, _) = get_raw_pos_info(loc.loc_end);
  /*let endchar = loc.loc_end.pos_cnum - loc.loc_start.pos_cnum + startchar in*/

  if (startchar >= 0) {
    if (line == endline) {
      print_endline(
        msg
        ++ " "
        ++ string_of_int(line)
        ++ ":"
        ++ string_of_int(startchar)
        ++ ","
        ++ string_of_int(endchar),
      );
    } else {
      print_endline(
        msg
        ++ " "
        ++ string_of_int(line)
        ++ ":"
        ++ string_of_int(startchar)
        ++ " - "
        ++ string_of_int(endline)
        ++ ":"
        ++ string_of_int(endchar),
      );
    };
  };
};

let getComment = (comment: Parsetree.comment) =>
  switch (comment) {
  | Line(cmt) => cmt.cmt_content
  | Block(cmt) => cmt.cmt_content
  | Doc(cmt) => cmt.cmt_content
  | Shebang(cmt) => cmt.cmt_content
  };

// let compare_locations =
//     (loc1: Grain_parsing.Location.t, loc2: Grain_parsing.Location.t) => {
//   let (_, raw1l, raw1c, _) = get_raw_pos_info(loc1.loc_start);
//   let (_, raw2l, raw2c, _) = get_raw_pos_info(loc2.loc_start);

//   let (_, raw1le, raw1ce, _) = get_raw_pos_info(loc1.loc_end);
//   let (_, raw2le, raw2ce, _) = get_raw_pos_info(loc2.loc_end);

//   // print_loc("compare loc1", loc1);
//   // print_loc("to loc2", loc2);

//   let loc2_starts_before =
//     if (raw2l < raw1l) {
//       true;
//     } else if (raw2l == raw1l) {
//       if (raw2c < raw1c) {
//         true;
//       } else {
//         false;
//       };
//     } else {
//       false;
//     };

//   // simple comparison

//   let loc2_starts_same =
//     if (raw2l == raw1l && raw2c == raw1c) {
//       true;
//     } else {
//       false;
//     };

//   let loc2_ends_same =
//     if (raw2le == raw1le && raw2ce == raw1ce) {
//       true;
//     } else {
//       false;
//     };

//   let res =
//     if (loc2_starts_same && loc2_ends_same) {
//       0;
//     } else if (loc2_starts_before) {
//       1;
//     } else {
//       (-1);
//     };

//   res;
// };

let comparePoints = (line1, char1, line2, char2) =>
  if (line1 == line2 && char1 == char2) {
    0;
  } else if (line1 <= line2) {
    if (line1 == line2) {
      if (char1 < char2) {
        (-1);
      } else {
        1;
      };
    } else {
      (-1);
    };
  } else {
    1;
  };

let compare_locations =
    (loc1: Grain_parsing.Location.t, loc2: Grain_parsing.Location.t) => {
  let (_, raw1l, raw1c, _) = get_raw_pos_info(loc1.loc_start);
  let (_, raw2l, raw2c, _) = get_raw_pos_info(loc2.loc_start);

  let (_, raw1le, raw1ce, _) = get_raw_pos_info(loc1.loc_end);
  let (_, raw2le, raw2ce, _) = get_raw_pos_info(loc2.loc_end);

  // print_loc("compare loc1", loc1);
  // print_loc("to loc2", loc2);

  // compare the leading points

  let res =
    if (comparePoints(raw1l, raw1c, raw2l, raw2c) == 0
        && comparePoints(raw1le, raw1ce, raw2le, raw2ce) == 0) {
      0;
    } else {
      comparePoints(raw1l, raw1c, raw2l, raw2c);
    };

  // print_endline("res is " ++ string_of_int(res));
  res;
};

let compare_partition_locations =
    (loc1: Grain_parsing.Location.t, loc2: Grain_parsing.Location.t) => {
  let (_, raw1l, raw1c, _) = get_raw_pos_info(loc1.loc_start);
  let (_, raw2l, raw2c, _) = get_raw_pos_info(loc2.loc_start);

  let (_, raw1le, raw1ce, _) = get_raw_pos_info(loc1.loc_end);
  let (_, raw2le, raw2ce, _) = get_raw_pos_info(loc2.loc_end);

  // print_loc("compare loc1", loc1);
  // print_loc("to loc2", loc2);

  // compare the leading points

  let res =
    if (comparePoints(raw1l, raw1c, raw2l, raw2c) == 0
        && comparePoints(raw1le, raw1ce, raw2le, raw2ce) == 0) {
      0;
    } else if
      // is loc2 inside loc1
      (comparePoints(raw1l, raw1c, raw2l, raw2c) < 1
       && comparePoints(raw2le, raw2ce, raw1le, raw1ce) < 1) {
      0;
    } else {
      comparePoints(raw1l, raw1c, raw2l, raw2c);
    };

  //print_endline("res is " ++ string_of_int(res));
  res;
};

let walktree =
    (
      statements: list(Grain_parsing.Parsetree.toplevel_stmt),
      comments: list(Grain_parsing.Parsetree.comment),
    ) => {
  let comment_locations =
    List.map(c => Comment((getCommentLoc(c), c)), comments);

  allLocations := comment_locations;

  let iter_location = (self, location) =>
    // print_loc("walked location:", location);
    if (!List.mem(Code(location), allLocations^)) {
      allLocations := List.append(allLocations^, [Code(location)]);
    };

  let iterator = {...Ast_iterator.default_iterator, location: iter_location};

  List.iter(iterator.toplevel(iterator), statements);

  allLocations :=
    List.sort(
      (node1: node_t, node2: node_t) => {
        let loc1 = getNodeLoc(node1);

        let loc2 = getNodeLoc(node2);

        compare_locations(loc1, loc2);
      },
      allLocations^,
    );
  // List.iter(
  //   n =>
  //     switch (n) {
  //     | Comment((l, c)) => print_loc("comment", l)
  //     | Code(l) => print_loc("code", l)
  //     },
  //   allLocations^,
  // );
};

let partitionNodeCommentsXX =
    (loc: Grain_parsing.Location.t, removeUsedTrailing: bool)
    : (
        list(Grain_parsing.Parsetree.comment),
        list(Grain_parsing.Parsetree.comment),
      ) => {
  print_loc("****** partitionNodeComments", loc);

  let (preceeding, following) =
    List.fold_left(
      (acc, n) => {
        let (preceeding, following) = acc;
        let nodeLoc = getNodeLoc(n);

        if (compare_partition_locations(loc, nodeLoc) > 0) {
          (preceeding @ [n], following);
        } else if (compare_partition_locations(loc, nodeLoc) < 0) {
          (preceeding, following @ [n]);
        } else {
          acc;
        };
      },
      ([], []),
      allLocations^,
    );

  // print_endline(
  //   "We have a preceeding count of "
  //   ++ string_of_int(List.length(preceeding)),
  // );

  // print_endline(
  //   "We have a following count of " ++ string_of_int(List.length(following)),
  // );

  let sortedPreceeding =
    List.sort(
      (node1: node_t, node2: node_t) => {
        let loc1 = getNodeLoc(node1);

        let loc2 = getNodeLoc(node2);

        compare_locations(loc1, loc2);
      },
      preceeding,
    );

  let sortedFollowing =
    List.sort(
      (node1: node_t, node2: node_t) => {
        let loc1 = getNodeLoc(node1);

        let loc2 = getNodeLoc(node2);

        compare_locations(loc1, loc2);
      },
      following,
    );

  List.iter(n => print_loc("pre", getNodeLoc(n)), sortedPreceeding);
  List.iter(n => print_loc("post", getNodeLoc(n)), sortedFollowing);

  let skip = ref(false);
  let preComments =
    List.fold_left(
      (acc, n) =>
        if (skip^) {
          acc;
        } else {
          switch (n) {
          | Comment((l, c)) => [c] @ acc
          | Code(_) =>
            skip := true;
            acc;
          };
        },
      [],
      List.rev(sortedPreceeding),
    );

  skip := false;
  let postComments =
    List.fold_left(
      (acc, n) =>
        if (skip^) {
          acc;
        } else {
          switch (n) {
          | Comment((l, c)) => acc @ [c]
          | Code(_) =>
            skip := true;
            acc;
          };
        },
      [],
      sortedFollowing,
    );

  // look for all comments after this node untl we hit a node not a comment

  // assume we use them so remove them

  let cleanedList =
    List.filter(
      n => {
        switch (n) {
        | Code(_) => true
        | Comment((loc, comment)) =>
          if (List.mem(comment, preComments)
              || removeUsedTrailing
              && List.mem(comment, postComments)) {
            false;
          } else {
            true;
          }
        }
      },
      allLocations^,
    );

  allLocations := cleanedList;

  // print_endline(
  //   "We finish with preceeding comments of  "
  //   ++ string_of_int(List.length(preComments)),
  // );

  // List.iter(c => print_endline(getComment(c)), preComments);

  // print_endline(
  //   "We finsh with a following comments of "
  //   ++ string_of_int(List.length(postComments)),
  // );

  // List.iter(c => print_endline(getComment(c)), postComments);

  (preComments, postComments);
};

let getLeadingComments =
    (loc: Grain_parsing.Location.t): list(Grain_parsing.Parsetree.comment) => {
  //print_loc("****** getLeadingComments", loc);

  let preceeding =
    List.fold_left(
      (acc, n) => {
        let preceeding = acc;
        let nodeLoc = getNodeLoc(n);

        if (compare_partition_locations(loc, nodeLoc) > 0) {
          preceeding @ [n];
        } else {
          acc;
        };
      },
      [],
      allLocations^,
    );

  // print_endline(
  //   "We have a preceeding count of "
  //   ++ string_of_int(List.length(preceeding)),
  // );

  // print_endline(
  //   "We have a following count of " ++ string_of_int(List.length(following)),
  // );

  let sortedPreceeding =
    List.sort(
      (node1: node_t, node2: node_t) => {
        let loc1 = getNodeLoc(node1);

        let loc2 = getNodeLoc(node2);

        compare_locations(loc1, loc2);
      },
      preceeding,
    );

  // List.iter(n => print_loc("pre", getNodeLoc(n)), sortedPreceeding);

  let skip = ref(false);
  let preComments =
    List.fold_left(
      (acc, n) =>
        if (skip^) {
          acc;
        } else {
          switch (n) {
          | Comment((l, c)) => [c] @ acc
          | Code(_) =>
            skip := true;
            acc;
          };
        },
      [],
      List.rev(sortedPreceeding),
    );

  // assume we use them so remove them

  let cleanedList =
    List.filter(
      n =>
        switch (n) {
        | Code(_) => true
        | Comment((loc, comment)) =>
          if (List.mem(comment, preComments)) {
            false;
          } else {
            true;
          }
        },
      allLocations^,
    );

  allLocations := cleanedList;

  // print_endline(
  //   "We finish with preceeding comments of  "
  //   ++ string_of_int(List.length(preComments)),
  // );

  // List.iter(c => print_endline(getComment(c)), preComments);

  preComments;
};

let getTrailingComments =
    (loc: Grain_parsing.Location.t): list(Grain_parsing.Parsetree.comment) => {
  //print_loc("****** getTrailingComments", loc);

  let following =
    List.fold_left(
      (acc, n) => {
        let following = acc;
        let nodeLoc = getNodeLoc(n);

        if (compare_partition_locations(loc, nodeLoc) < 0) {
          following @ [n];
        } else {
          acc;
        };
      },
      [],
      allLocations^,
    );

  let sortedFollowing =
    List.sort(
      (node1: node_t, node2: node_t) => {
        let loc1 = getNodeLoc(node1);

        let loc2 = getNodeLoc(node2);

        compare_locations(loc1, loc2);
      },
      following,
    );

  // List.iter(n => print_loc("post", getNodeLoc(n)), sortedFollowing);

  let skip = ref(false);

  let postComments =
    List.fold_left(
      (acc, n) =>
        if (skip^) {
          acc;
        } else {
          switch (n) {
          | Comment((l, c)) => acc @ [c]
          | Code(_) =>
            skip := true;
            acc;
          };
        },
      [],
      sortedFollowing,
    );

  // look for all comments after this node untl we hit a node not a comment

  // assume we use them so remove them

  let cleanedList =
    List.filter(
      n => {
        switch (n) {
        | Code(_) => true
        | Comment((loc, comment)) =>
          if (List.mem(comment, postComments)) {
            false;
          } else {
            true;
          }
        }
      },
      allLocations^,
    );

  allLocations := cleanedList;

  // print_endline(
  //   "We finsh with a following comments of "
  //   ++ string_of_int(List.length(postComments)),
  // );

  // List.iter(c => print_endline(getComment(c)), postComments);

  postComments;
};