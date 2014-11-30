#include "convert2bed.h"

int
main(int argc, char **argv)
{
#ifdef DEBUG
    fprintf (stderr, "--- convert2bed main() - enter ---\n");
#endif

    struct stat stats;
    int stats_res;
    c2b_pipeset_t pipes;

    /* setup */
    c2b_init_globals();
    c2b_init_command_line_options(argc, argv);
    /* check that stdin is available */
    if ((stats_res = fstat(fileno(stdin), &stats)) < 0) {
        fprintf(stderr, "Error: fstat() call failed");
        c2b_print_usage(stderr);
        return EXIT_FAILURE;
    }
    if ((S_ISCHR(stats.st_mode) == kTrue) && (S_ISREG(stats.st_mode) == kFalse)) {
        fprintf(stderr, "Error: No input is specified; please redirect or pipe in formatted data\n");
        c2b_print_usage(stderr);
        return EXIT_FAILURE;
    }
    c2b_test_dependencies();
    c2b_init_pipeset(&pipes, MAX_PIPES);

    /* convert */
    c2b_init_conversion(&pipes);

    /* clean-up */
    c2b_delete_pipeset(&pipes);
    c2b_delete_globals();

#ifdef DEBUG
    fprintf (stderr, "--- convert2bed main() - exit  ---\n");
#endif
    return EXIT_SUCCESS;
}

static void
c2b_init_conversion(c2b_pipeset_t *p)
{
#ifdef DEBUG
    fprintf(stderr, "--- c2b_init_conversion() - enter ---\n");
#endif

    switch(c2b_globals.input_format_idx)
        {
        case BAM_FORMAT:
            c2b_init_bam_conversion(p);
            break;
        case GFF_FORMAT:
            c2b_init_gff_conversion(p);
            break;
        case GTF_FORMAT:
            c2b_init_gtf_conversion(p);
            break;
        case PSL_FORMAT:
            c2b_init_psl_conversion(p);
            break;
        case SAM_FORMAT:
            c2b_init_sam_conversion(p);
            break;
        default:
            fprintf(stderr, "Error: Currently unsupported format\n");
            c2b_print_usage(stderr);
            exit(EXIT_FAILURE);
        }

#ifdef DEBUG
    fprintf(stderr, "--- c2b_init_conversion() - exit  ---\n");
#endif
}

static void
c2b_init_bam_conversion(c2b_pipeset_t *p)
{
#ifdef DEBUG
    fprintf(stderr, "--- c2b_init_bam_conversion() - enter ---\n");
#endif

    pthread_t bam2sam_thread; 
    pthread_t sam2bed_unsorted_thread; 
    pthread_t bed_unsorted2stdout_thread;
    pthread_t bed_unsorted2bed_sorted_thread;
    pthread_t bed_sorted2stdout_thread;
    pthread_t bed_sorted2starch_thread;
    pthread_t starch2stdout_thread;
    pid_t bam2sam_proc;
    pid_t bed_unsorted2bed_sorted_proc;
    pid_t bed_sorted2starch_proc;
    c2b_pipeline_stage_t bam2sam_stage;
    c2b_pipeline_stage_t sam2bed_unsorted_stage;
    c2b_pipeline_stage_t bed_unsorted2stdout_stage;
    c2b_pipeline_stage_t bed_unsorted2bed_sorted_stage;
    c2b_pipeline_stage_t bed_sorted2stdout_stage;
    c2b_pipeline_stage_t bed_sorted2starch_stage;
    c2b_pipeline_stage_t starch2stdout_stage;
    char bam2sam_cmd[MAX_LINE_LENGTH_VALUE] = {0};
    char bed_unsorted2bed_sorted_cmd[MAX_LINE_LENGTH_VALUE] = {0};
    char bed_sorted2starch_cmd[MAX_LINE_LENGTH_VALUE] = {0};
    void (*sam2bed_unsorted_line_functor)(char *, ssize_t *, char *, ssize_t) = NULL;

    sam2bed_unsorted_line_functor = (!c2b_globals.split_flag ?
                                     c2b_line_convert_sam_to_bed_unsorted_without_split_operation :
                                     c2b_line_convert_sam_to_bed_unsorted_with_split_operation);

    if ((!c2b_globals.sort_flag) && (c2b_globals.output_format_idx == BED_FORMAT)) {
        bam2sam_stage.pipeset = p;
        bam2sam_stage.line_functor = NULL;
        bam2sam_stage.src = -1; /* src is really stdin */
        bam2sam_stage.dest = 0;
        
        sam2bed_unsorted_stage.pipeset = p;
        sam2bed_unsorted_stage.line_functor = sam2bed_unsorted_line_functor;
        sam2bed_unsorted_stage.src = 0;
        sam2bed_unsorted_stage.dest = 1;

        bed_unsorted2stdout_stage.pipeset = p;
        bed_unsorted2stdout_stage.line_functor = NULL;
        bed_unsorted2stdout_stage.src = 1;
        bed_unsorted2stdout_stage.dest = -1; /* dest BED is stdout */
    }
    else if (c2b_globals.output_format_idx == BED_FORMAT) {
        bam2sam_stage.pipeset = p;
        bam2sam_stage.line_functor = NULL;
        bam2sam_stage.src = -1; /* src is really stdin */
        bam2sam_stage.dest = 0;
        
        sam2bed_unsorted_stage.pipeset = p;
        sam2bed_unsorted_stage.line_functor = sam2bed_unsorted_line_functor;
        sam2bed_unsorted_stage.src = 0;
        sam2bed_unsorted_stage.dest = 1;
        
        bed_unsorted2bed_sorted_stage.pipeset = p;
        bed_unsorted2bed_sorted_stage.line_functor = NULL;
        bed_unsorted2bed_sorted_stage.src = 1;
        bed_unsorted2bed_sorted_stage.dest = 2;

        bed_sorted2stdout_stage.pipeset = p;
        bed_sorted2stdout_stage.line_functor = NULL;
        bed_sorted2stdout_stage.src = 2;
        bed_sorted2stdout_stage.dest = -1; /* dest BED is stdout */
    }
    else if (c2b_globals.output_format_idx == STARCH_FORMAT) {
        bam2sam_stage.pipeset = p;
        bam2sam_stage.line_functor = NULL;
        bam2sam_stage.src = -1; /* src is really stdin */
        bam2sam_stage.dest = 0;
        
        sam2bed_unsorted_stage.pipeset = p;
        sam2bed_unsorted_stage.line_functor = sam2bed_unsorted_line_functor;
        sam2bed_unsorted_stage.src = 0;
        sam2bed_unsorted_stage.dest = 1;

        bed_unsorted2bed_sorted_stage.pipeset = p;
        bed_unsorted2bed_sorted_stage.line_functor = NULL;
        bed_unsorted2bed_sorted_stage.src = 1;
        bed_unsorted2bed_sorted_stage.dest = 2;

        bed_sorted2starch_stage.pipeset = p;
        bed_sorted2starch_stage.line_functor = NULL;
        bed_sorted2starch_stage.src = 2;
        bed_sorted2starch_stage.dest = 3;

        starch2stdout_stage.pipeset = p;
        starch2stdout_stage.line_functor = NULL;
        starch2stdout_stage.src = 3;
        starch2stdout_stage.dest = -1; /* dest Starch is stdout */
    }
    else {
        fprintf(stderr, "Error: Unknown BAM conversion parameter combination\n");
        c2b_print_usage(stderr);
        exit(EXIT_FAILURE);
    }

    /*
       We open pid_t (process) instances to handle data in a specified order. 
    */

    c2b_cmd_bam_to_sam(bam2sam_cmd);
#ifdef DEBUG
    fprintf(stderr, "Debug: c2b_cmd_bam_to_sam: [%s]\n", bam2sam_cmd);
#endif
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunknown-pragmas"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
    bam2sam_proc = c2b_popen4(bam2sam_cmd,
			      p->in[0],
			      p->out[0],
			      p->err[0],
			      POPEN4_FLAG_NONE);

#pragma GCC diagnostic pop

    if (c2b_globals.sort_flag) {
        c2b_cmd_sort_bed(bed_unsorted2bed_sorted_cmd);
#ifdef DEBUG
        fprintf(stderr, "Debug: c2b_cmd_sort_bed: [%s]\n", bed_unsorted2bed_sorted_cmd);
#endif
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunknown-pragmas"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
        bed_unsorted2bed_sorted_proc = c2b_popen4(bed_unsorted2bed_sorted_cmd,
                                                  p->in[2],
                                                  p->out[2],
                                                  p->err[2],
                                                  POPEN4_FLAG_NONE);
#pragma GCC diagnostic pop
    }

    if (c2b_globals.output_format_idx == STARCH_FORMAT) {
        c2b_cmd_starch_bed(bed_sorted2starch_cmd);
#ifdef DEBUG
        fprintf(stderr, "Debug: c2b_cmd_starch_bed: [%s]\n", bed_sorted2starch_cmd);
#endif
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunknown-pragmas"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
        bed_sorted2starch_proc = c2b_popen4(bed_sorted2starch_cmd,
                                            p->in[3],
                                            p->out[3],
                                            p->err[3],
                                            POPEN4_FLAG_NONE);
#pragma GCC diagnostic pop
    }

#ifdef DEBUG
    c2b_debug_pipeset(p, MAX_PIPES);
#endif

    /*
       Once we have the desired process instances, we create and join
       threads for their ordered execution.
    */

    if ((!c2b_globals.sort_flag) && (c2b_globals.output_format_idx == BED_FORMAT)) {
        pthread_create(&bam2sam_thread,
                       NULL,
                       c2b_read_bytes_from_stdin,
                       &bam2sam_stage);
        pthread_create(&sam2bed_unsorted_thread,
                       NULL,
                       c2b_process_intermediate_bytes_by_lines,
                       &sam2bed_unsorted_stage);
        pthread_create(&bed_unsorted2stdout_thread,
                       NULL,
                       c2b_write_in_bytes_to_stdout,
                       &bed_unsorted2stdout_stage);
    }
    else if (c2b_globals.output_format_idx == BED_FORMAT) {
        pthread_create(&bam2sam_thread,
                       NULL,
                       c2b_read_bytes_from_stdin,
                       &bam2sam_stage);
        pthread_create(&sam2bed_unsorted_thread,
                       NULL,
                       c2b_process_intermediate_bytes_by_lines,
                       &sam2bed_unsorted_stage);
        pthread_create(&bed_unsorted2bed_sorted_thread,
                       NULL,
                       c2b_write_in_bytes_to_in_process,
                       &bed_unsorted2bed_sorted_stage);
        pthread_create(&bed_sorted2stdout_thread,
                       NULL,
                       c2b_write_out_bytes_to_stdout,
                       &bed_sorted2stdout_stage);
    }
    else if (c2b_globals.output_format_idx == STARCH_FORMAT) {
        pthread_create(&bam2sam_thread,
                       NULL,
                       c2b_read_bytes_from_stdin,
                       &bam2sam_stage);
        pthread_create(&sam2bed_unsorted_thread,
                       NULL,
                       c2b_process_intermediate_bytes_by_lines,
                       &sam2bed_unsorted_stage);
        pthread_create(&bed_unsorted2bed_sorted_thread,
                       NULL,
                       c2b_write_in_bytes_to_in_process,
                       &bed_unsorted2bed_sorted_stage);
        pthread_create(&bed_sorted2starch_thread,
                       NULL,
                       c2b_write_out_bytes_to_in_process,
                       &bed_sorted2starch_stage);
        pthread_create(&starch2stdout_thread,
                       NULL,
                       c2b_write_out_bytes_to_stdout,
                       &starch2stdout_stage);
    }

    if ((!c2b_globals.sort_flag) && (c2b_globals.output_format_idx == BED_FORMAT)) {
        pthread_join(bam2sam_thread, (void **) NULL);
        pthread_join(sam2bed_unsorted_thread, (void **) NULL);
        pthread_join(bed_unsorted2stdout_thread, (void **) NULL);
    }
    else if (c2b_globals.output_format_idx == BED_FORMAT) {
        pthread_join(bam2sam_thread, (void **) NULL);
        pthread_join(sam2bed_unsorted_thread, (void **) NULL);
        pthread_join(bed_unsorted2bed_sorted_thread, (void **) NULL);
        pthread_join(bed_sorted2stdout_thread, (void **) NULL);
    }
    else if (c2b_globals.output_format_idx == STARCH_FORMAT) {
        pthread_join(bam2sam_thread, (void **) NULL);
        pthread_join(sam2bed_unsorted_thread, (void **) NULL);
        pthread_join(bed_unsorted2bed_sorted_thread, (void **) NULL);
        pthread_join(bed_sorted2starch_thread, (void **) NULL);
        pthread_join(starch2stdout_thread, (void **) NULL);
    }

#ifdef DEBUG
    fprintf(stderr, "--- c2b_init_bam_conversion() - exit  ---\n");
#endif
}

static void
c2b_init_gff_conversion(c2b_pipeset_t *p)
{
#ifdef DEBUG
    fprintf(stderr, "--- c2b_init_gff_conversion() - enter ---\n");
#endif

    pthread_t cat2gff_thread; 
    pthread_t gff2bed_unsorted_thread; 
    pthread_t bed_unsorted2stdout_thread;
    pthread_t bed_unsorted2bed_sorted_thread;
    pthread_t bed_sorted2stdout_thread;
    pthread_t bed_sorted2starch_thread;
    pthread_t starch2stdout_thread;
    pid_t cat2gff_proc;
    pid_t bed_unsorted2bed_sorted_proc;
    pid_t bed_sorted2starch_proc;
    c2b_pipeline_stage_t cat2gff_stage;
    c2b_pipeline_stage_t gff2bed_unsorted_stage;
    c2b_pipeline_stage_t bed_unsorted2stdout_stage;
    c2b_pipeline_stage_t bed_unsorted2bed_sorted_stage;
    c2b_pipeline_stage_t bed_sorted2stdout_stage;
    c2b_pipeline_stage_t bed_sorted2starch_stage;
    c2b_pipeline_stage_t starch2stdout_stage;
    char cat2gff_cmd[MAX_LINE_LENGTH_VALUE] = {0};
    char bed_unsorted2bed_sorted_cmd[MAX_LINE_LENGTH_VALUE] = {0};
    char bed_sorted2starch_cmd[MAX_LINE_LENGTH_VALUE] = {0};
    void (*gff2bed_unsorted_line_functor)(char *, ssize_t *, char *, ssize_t) = NULL;

    gff2bed_unsorted_line_functor = c2b_line_convert_gff_to_bed_unsorted;

    if ((!c2b_globals.sort_flag) && (c2b_globals.output_format_idx == BED_FORMAT)) {
        cat2gff_stage.pipeset = p;
        cat2gff_stage.line_functor = NULL;
        cat2gff_stage.src = -1; /* src is really stdin */
        cat2gff_stage.dest = 0;
        
        gff2bed_unsorted_stage.pipeset = p;
        gff2bed_unsorted_stage.line_functor = gff2bed_unsorted_line_functor;
        gff2bed_unsorted_stage.src = 0;
        gff2bed_unsorted_stage.dest = 1;

        bed_unsorted2stdout_stage.pipeset = p;
        bed_unsorted2stdout_stage.line_functor = NULL;
        bed_unsorted2stdout_stage.src = 1;
        bed_unsorted2stdout_stage.dest = -1; /* dest BED is stdout */
    }
    else if (c2b_globals.output_format_idx == BED_FORMAT) {
        cat2gff_stage.pipeset = p;
        cat2gff_stage.line_functor = NULL;
        cat2gff_stage.src = -1; /* src is really stdin */
        cat2gff_stage.dest = 0;
        
        gff2bed_unsorted_stage.pipeset = p;
        gff2bed_unsorted_stage.line_functor = gff2bed_unsorted_line_functor;
        gff2bed_unsorted_stage.src = 0;
        gff2bed_unsorted_stage.dest = 1;
        
        bed_unsorted2bed_sorted_stage.pipeset = p;
        bed_unsorted2bed_sorted_stage.line_functor = NULL;
        bed_unsorted2bed_sorted_stage.src = 1;
        bed_unsorted2bed_sorted_stage.dest = 2;

        bed_sorted2stdout_stage.pipeset = p;
        bed_sorted2stdout_stage.line_functor = NULL;
        bed_sorted2stdout_stage.src = 2;
        bed_sorted2stdout_stage.dest = -1; /* dest BED is stdout */
    }
    else if (c2b_globals.output_format_idx == STARCH_FORMAT) {
        cat2gff_stage.pipeset = p;
        cat2gff_stage.line_functor = NULL;
        cat2gff_stage.src = -1; /* src is really stdin */
        cat2gff_stage.dest = 0;
        
        gff2bed_unsorted_stage.pipeset = p;
        gff2bed_unsorted_stage.line_functor = gff2bed_unsorted_line_functor;
        gff2bed_unsorted_stage.src = 0;
        gff2bed_unsorted_stage.dest = 1;

        bed_unsorted2bed_sorted_stage.pipeset = p;
        bed_unsorted2bed_sorted_stage.line_functor = NULL;
        bed_unsorted2bed_sorted_stage.src = 1;
        bed_unsorted2bed_sorted_stage.dest = 2;

        bed_sorted2starch_stage.pipeset = p;
        bed_sorted2starch_stage.line_functor = NULL;
        bed_sorted2starch_stage.src = 2;
        bed_sorted2starch_stage.dest = 3;

        starch2stdout_stage.pipeset = p;
        starch2stdout_stage.line_functor = NULL;
        starch2stdout_stage.src = 3;
        starch2stdout_stage.dest = -1; /* dest Starch is stdout */
    }
    else {
        fprintf(stderr, "Error: Unknown GFF conversion parameter combination\n");
        c2b_print_usage(stderr);
        exit(EXIT_FAILURE);
    }

    /*
       We open pid_t (process) instances to handle data in a specified order. 
    */

    c2b_cmd_cat_stdin(cat2gff_cmd);
#ifdef DEBUG
    fprintf(stderr, "Debug: c2b_cmd_cat_stdin: [%s]\n", cat2gff_cmd);
#endif
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunknown-pragmas"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"

    cat2gff_proc = c2b_popen4(cat2gff_cmd,
			      p->in[0],
			      p->out[0],
			      p->err[0],
			      POPEN4_FLAG_NONE);

#pragma GCC diagnostic pop

    if (c2b_globals.sort_flag) {
        c2b_cmd_sort_bed(bed_unsorted2bed_sorted_cmd);
#ifdef DEBUG
        fprintf(stderr, "Debug: c2b_cmd_sort_bed: [%s]\n", bed_unsorted2bed_sorted_cmd);
#endif
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunknown-pragmas"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
        bed_unsorted2bed_sorted_proc = c2b_popen4(bed_unsorted2bed_sorted_cmd,
                                                  p->in[2],
                                                  p->out[2],
                                                  p->err[2],
                                                  POPEN4_FLAG_NONE);
#pragma GCC diagnostic pop
    }

    if (c2b_globals.output_format_idx == STARCH_FORMAT) {
        c2b_cmd_starch_bed(bed_sorted2starch_cmd);
#ifdef DEBUG
        fprintf(stderr, "Debug: c2b_cmd_starch_bed: [%s]\n", bed_sorted2starch_cmd);
#endif
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunknown-pragmas"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
        bed_sorted2starch_proc = c2b_popen4(bed_sorted2starch_cmd,
                                            p->in[3],
                                            p->out[3],
                                            p->err[3],
                                            POPEN4_FLAG_NONE);
#pragma GCC diagnostic pop
    }

#ifdef DEBUG
    c2b_debug_pipeset(p, MAX_PIPES);
#endif

    /*
       Once we have the desired process instances, we create and join
       threads for their ordered execution.
    */

    if ((!c2b_globals.sort_flag) && (c2b_globals.output_format_idx == BED_FORMAT)) {
        pthread_create(&cat2gff_thread,
                       NULL,
                       c2b_read_bytes_from_stdin,
                       &cat2gff_stage);
        pthread_create(&gff2bed_unsorted_thread,
                       NULL,
                       c2b_process_intermediate_bytes_by_lines,
                       &gff2bed_unsorted_stage);
        pthread_create(&bed_unsorted2stdout_thread,
                       NULL,
                       c2b_write_in_bytes_to_stdout,
                       &bed_unsorted2stdout_stage);
    }
    else if (c2b_globals.output_format_idx == BED_FORMAT) {
        pthread_create(&cat2gff_thread,
                       NULL,
                       c2b_read_bytes_from_stdin,
                       &cat2gff_stage);
        pthread_create(&gff2bed_unsorted_thread,
                       NULL,
                       c2b_process_intermediate_bytes_by_lines,
                       &gff2bed_unsorted_stage);
        pthread_create(&bed_unsorted2bed_sorted_thread,
                       NULL,
                       c2b_write_in_bytes_to_in_process,
                       &bed_unsorted2bed_sorted_stage);
        pthread_create(&bed_sorted2stdout_thread,
                       NULL,
                       c2b_write_out_bytes_to_stdout,
                       &bed_sorted2stdout_stage);
    }
    else if (c2b_globals.output_format_idx == STARCH_FORMAT) {
        pthread_create(&cat2gff_thread,
                       NULL,
                       c2b_read_bytes_from_stdin,
                       &cat2gff_stage);
        pthread_create(&gff2bed_unsorted_thread,
                       NULL,
                       c2b_process_intermediate_bytes_by_lines,
                       &gff2bed_unsorted_stage);
        pthread_create(&bed_unsorted2bed_sorted_thread,
                       NULL,
                       c2b_write_in_bytes_to_in_process,
                       &bed_unsorted2bed_sorted_stage);
        pthread_create(&bed_sorted2starch_thread,
                       NULL,
                       c2b_write_out_bytes_to_in_process,
                       &bed_sorted2starch_stage);
        pthread_create(&starch2stdout_thread,
                       NULL,
                       c2b_write_out_bytes_to_stdout,
                       &starch2stdout_stage);
    }

    if ((!c2b_globals.sort_flag) && (c2b_globals.output_format_idx == BED_FORMAT)) {
        pthread_join(cat2gff_thread, (void **) NULL);
        pthread_join(gff2bed_unsorted_thread, (void **) NULL);
        pthread_join(bed_unsorted2stdout_thread, (void **) NULL);
    }
    else if (c2b_globals.output_format_idx == BED_FORMAT) {
        pthread_join(cat2gff_thread, (void **) NULL);
        pthread_join(gff2bed_unsorted_thread, (void **) NULL);
        pthread_join(bed_unsorted2bed_sorted_thread, (void **) NULL);
        pthread_join(bed_sorted2stdout_thread, (void **) NULL);
    }
    else if (c2b_globals.output_format_idx == STARCH_FORMAT) {
        pthread_join(cat2gff_thread, (void **) NULL);
        pthread_join(gff2bed_unsorted_thread, (void **) NULL);
        pthread_join(bed_unsorted2bed_sorted_thread, (void **) NULL);
        pthread_join(bed_sorted2starch_thread, (void **) NULL);
        pthread_join(starch2stdout_thread, (void **) NULL);
    }

#ifdef DEBUG
    fprintf(stderr, "--- c2b_init_gff_conversion() - exit  ---\n");
#endif
}

static void
c2b_init_gtf_conversion(c2b_pipeset_t *p)
{
#ifdef DEBUG
    fprintf(stderr, "--- c2b_init_gtf_conversion() - enter ---\n");
#endif

    pthread_t cat2gtf_thread; 
    pthread_t gtf2bed_unsorted_thread; 
    pthread_t bed_unsorted2stdout_thread;
    pthread_t bed_unsorted2bed_sorted_thread;
    pthread_t bed_sorted2stdout_thread;
    pthread_t bed_sorted2starch_thread;
    pthread_t starch2stdout_thread;
    pid_t cat2gtf_proc;
    pid_t bed_unsorted2bed_sorted_proc;
    pid_t bed_sorted2starch_proc;
    c2b_pipeline_stage_t cat2gtf_stage;
    c2b_pipeline_stage_t gtf2bed_unsorted_stage;
    c2b_pipeline_stage_t bed_unsorted2stdout_stage;
    c2b_pipeline_stage_t bed_unsorted2bed_sorted_stage;
    c2b_pipeline_stage_t bed_sorted2stdout_stage;
    c2b_pipeline_stage_t bed_sorted2starch_stage;
    c2b_pipeline_stage_t starch2stdout_stage;
    char cat2gtf_cmd[MAX_LINE_LENGTH_VALUE] = {0};
    char bed_unsorted2bed_sorted_cmd[MAX_LINE_LENGTH_VALUE] = {0};
    char bed_sorted2starch_cmd[MAX_LINE_LENGTH_VALUE] = {0};
    void (*gtf2bed_unsorted_line_functor)(char *, ssize_t *, char *, ssize_t) = NULL;

    gtf2bed_unsorted_line_functor = c2b_line_convert_gtf_to_bed_unsorted;

    if ((!c2b_globals.sort_flag) && (c2b_globals.output_format_idx == BED_FORMAT)) {
        cat2gtf_stage.pipeset = p;
        cat2gtf_stage.line_functor = NULL;
        cat2gtf_stage.src = -1; /* src is really stdin */
        cat2gtf_stage.dest = 0;
        
        gtf2bed_unsorted_stage.pipeset = p;
        gtf2bed_unsorted_stage.line_functor = gtf2bed_unsorted_line_functor;
        gtf2bed_unsorted_stage.src = 0;
        gtf2bed_unsorted_stage.dest = 1;

        bed_unsorted2stdout_stage.pipeset = p;
        bed_unsorted2stdout_stage.line_functor = NULL;
        bed_unsorted2stdout_stage.src = 1;
        bed_unsorted2stdout_stage.dest = -1; /* dest BED is stdout */
    }
    else if (c2b_globals.output_format_idx == BED_FORMAT) {
        cat2gtf_stage.pipeset = p;
        cat2gtf_stage.line_functor = NULL;
        cat2gtf_stage.src = -1; /* src is really stdin */
        cat2gtf_stage.dest = 0;
        
        gtf2bed_unsorted_stage.pipeset = p;
        gtf2bed_unsorted_stage.line_functor = gtf2bed_unsorted_line_functor;
        gtf2bed_unsorted_stage.src = 0;
        gtf2bed_unsorted_stage.dest = 1;
        
        bed_unsorted2bed_sorted_stage.pipeset = p;
        bed_unsorted2bed_sorted_stage.line_functor = NULL;
        bed_unsorted2bed_sorted_stage.src = 1;
        bed_unsorted2bed_sorted_stage.dest = 2;

        bed_sorted2stdout_stage.pipeset = p;
        bed_sorted2stdout_stage.line_functor = NULL;
        bed_sorted2stdout_stage.src = 2;
        bed_sorted2stdout_stage.dest = -1; /* dest BED is stdout */
    }
    else if (c2b_globals.output_format_idx == STARCH_FORMAT) {
        cat2gtf_stage.pipeset = p;
        cat2gtf_stage.line_functor = NULL;
        cat2gtf_stage.src = -1; /* src is really stdin */
        cat2gtf_stage.dest = 0;
        
        gtf2bed_unsorted_stage.pipeset = p;
        gtf2bed_unsorted_stage.line_functor = gtf2bed_unsorted_line_functor;
        gtf2bed_unsorted_stage.src = 0;
        gtf2bed_unsorted_stage.dest = 1;

        bed_unsorted2bed_sorted_stage.pipeset = p;
        bed_unsorted2bed_sorted_stage.line_functor = NULL;
        bed_unsorted2bed_sorted_stage.src = 1;
        bed_unsorted2bed_sorted_stage.dest = 2;

        bed_sorted2starch_stage.pipeset = p;
        bed_sorted2starch_stage.line_functor = NULL;
        bed_sorted2starch_stage.src = 2;
        bed_sorted2starch_stage.dest = 3;

        starch2stdout_stage.pipeset = p;
        starch2stdout_stage.line_functor = NULL;
        starch2stdout_stage.src = 3;
        starch2stdout_stage.dest = -1; /* dest Starch is stdout */
    }
    else {
        fprintf(stderr, "Error: Unknown GTF conversion parameter combination\n");
        c2b_print_usage(stderr);
        exit(EXIT_FAILURE);
    }

    /*
       We open pid_t (process) instances to handle data in a specified order. 
    */

    c2b_cmd_cat_stdin(cat2gtf_cmd);
#ifdef DEBUG
    fprintf(stderr, "Debug: c2b_cmd_cat_stdin: [%s]\n", cat2gtf_cmd);
#endif
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunknown-pragmas"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"

    cat2gtf_proc = c2b_popen4(cat2gtf_cmd,
			      p->in[0],
			      p->out[0],
			      p->err[0],
			      POPEN4_FLAG_NONE);

#pragma GCC diagnostic pop

    if (c2b_globals.sort_flag) {
        c2b_cmd_sort_bed(bed_unsorted2bed_sorted_cmd);
#ifdef DEBUG
        fprintf(stderr, "Debug: c2b_cmd_sort_bed: [%s]\n", bed_unsorted2bed_sorted_cmd);
#endif
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunknown-pragmas"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
        bed_unsorted2bed_sorted_proc = c2b_popen4(bed_unsorted2bed_sorted_cmd,
                                                  p->in[2],
                                                  p->out[2],
                                                  p->err[2],
                                                  POPEN4_FLAG_NONE);
#pragma GCC diagnostic pop
    }

    if (c2b_globals.output_format_idx == STARCH_FORMAT) {
        c2b_cmd_starch_bed(bed_sorted2starch_cmd);
#ifdef DEBUG
        fprintf(stderr, "Debug: c2b_cmd_starch_bed: [%s]\n", bed_sorted2starch_cmd);
#endif
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunknown-pragmas"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
        bed_sorted2starch_proc = c2b_popen4(bed_sorted2starch_cmd,
                                            p->in[3],
                                            p->out[3],
                                            p->err[3],
                                            POPEN4_FLAG_NONE);
#pragma GCC diagnostic pop
    }

#ifdef DEBUG
    c2b_debug_pipeset(p, MAX_PIPES);
#endif

    /*
       Once we have the desired process instances, we create and join
       threads for their ordered execution.
    */

    if ((!c2b_globals.sort_flag) && (c2b_globals.output_format_idx == BED_FORMAT)) {
        pthread_create(&cat2gtf_thread,
                       NULL,
                       c2b_read_bytes_from_stdin,
                       &cat2gtf_stage);
        pthread_create(&gtf2bed_unsorted_thread,
                       NULL,
                       c2b_process_intermediate_bytes_by_lines,
                       &gtf2bed_unsorted_stage);
        pthread_create(&bed_unsorted2stdout_thread,
                       NULL,
                       c2b_write_in_bytes_to_stdout,
                       &bed_unsorted2stdout_stage);
    }
    else if (c2b_globals.output_format_idx == BED_FORMAT) {
        pthread_create(&cat2gtf_thread,
                       NULL,
                       c2b_read_bytes_from_stdin,
                       &cat2gtf_stage);
        pthread_create(&gtf2bed_unsorted_thread,
                       NULL,
                       c2b_process_intermediate_bytes_by_lines,
                       &gtf2bed_unsorted_stage);
        pthread_create(&bed_unsorted2bed_sorted_thread,
                       NULL,
                       c2b_write_in_bytes_to_in_process,
                       &bed_unsorted2bed_sorted_stage);
        pthread_create(&bed_sorted2stdout_thread,
                       NULL,
                       c2b_write_out_bytes_to_stdout,
                       &bed_sorted2stdout_stage);
    }
    else if (c2b_globals.output_format_idx == STARCH_FORMAT) {
        pthread_create(&cat2gtf_thread,
                       NULL,
                       c2b_read_bytes_from_stdin,
                       &cat2gtf_stage);
        pthread_create(&gtf2bed_unsorted_thread,
                       NULL,
                       c2b_process_intermediate_bytes_by_lines,
                       &gtf2bed_unsorted_stage);
        pthread_create(&bed_unsorted2bed_sorted_thread,
                       NULL,
                       c2b_write_in_bytes_to_in_process,
                       &bed_unsorted2bed_sorted_stage);
        pthread_create(&bed_sorted2starch_thread,
                       NULL,
                       c2b_write_out_bytes_to_in_process,
                       &bed_sorted2starch_stage);
        pthread_create(&starch2stdout_thread,
                       NULL,
                       c2b_write_out_bytes_to_stdout,
                       &starch2stdout_stage);
    }

    if ((!c2b_globals.sort_flag) && (c2b_globals.output_format_idx == BED_FORMAT)) {
        pthread_join(cat2gtf_thread, (void **) NULL);
        pthread_join(gtf2bed_unsorted_thread, (void **) NULL);
        pthread_join(bed_unsorted2stdout_thread, (void **) NULL);
    }
    else if (c2b_globals.output_format_idx == BED_FORMAT) {
        pthread_join(cat2gtf_thread, (void **) NULL);
        pthread_join(gtf2bed_unsorted_thread, (void **) NULL);
        pthread_join(bed_unsorted2bed_sorted_thread, (void **) NULL);
        pthread_join(bed_sorted2stdout_thread, (void **) NULL);
    }
    else if (c2b_globals.output_format_idx == STARCH_FORMAT) {
        pthread_join(cat2gtf_thread, (void **) NULL);
        pthread_join(gtf2bed_unsorted_thread, (void **) NULL);
        pthread_join(bed_unsorted2bed_sorted_thread, (void **) NULL);
        pthread_join(bed_sorted2starch_thread, (void **) NULL);
        pthread_join(starch2stdout_thread, (void **) NULL);
    }

#ifdef DEBUG
    fprintf(stderr, "--- c2b_init_gtf_conversion() - exit  ---\n");
#endif
}

static void
c2b_init_psl_conversion(c2b_pipeset_t *p)
{
#ifdef DEBUG
    fprintf(stderr, "--- c2b_init_psl_conversion() - enter ---\n");
#endif

    pthread_t cat2psl_thread; 
    pthread_t psl2bed_unsorted_thread; 
    pthread_t bed_unsorted2stdout_thread;
    pthread_t bed_unsorted2bed_sorted_thread;
    pthread_t bed_sorted2stdout_thread;
    pthread_t bed_sorted2starch_thread;
    pthread_t starch2stdout_thread;
    pid_t cat2psl_proc;
    pid_t bed_unsorted2bed_sorted_proc;
    pid_t bed_sorted2starch_proc;
    c2b_pipeline_stage_t cat2psl_stage;
    c2b_pipeline_stage_t psl2bed_unsorted_stage;
    c2b_pipeline_stage_t bed_unsorted2stdout_stage;
    c2b_pipeline_stage_t bed_unsorted2bed_sorted_stage;
    c2b_pipeline_stage_t bed_sorted2stdout_stage;
    c2b_pipeline_stage_t bed_sorted2starch_stage;
    c2b_pipeline_stage_t starch2stdout_stage;
    char cat2psl_cmd[MAX_LINE_LENGTH_VALUE] = {0};
    char bed_unsorted2bed_sorted_cmd[MAX_LINE_LENGTH_VALUE] = {0};
    char bed_sorted2starch_cmd[MAX_LINE_LENGTH_VALUE] = {0};
    void (*psl2bed_unsorted_line_functor)(char *, ssize_t *, char *, ssize_t) = NULL;

    psl2bed_unsorted_line_functor = c2b_line_convert_psl_to_bed_unsorted;

    if ((!c2b_globals.sort_flag) && (c2b_globals.output_format_idx == BED_FORMAT)) {
        cat2psl_stage.pipeset = p;
        cat2psl_stage.line_functor = NULL;
        cat2psl_stage.src = -1; /* src is really stdin */
        cat2psl_stage.dest = 0;
        
        psl2bed_unsorted_stage.pipeset = p;
        psl2bed_unsorted_stage.line_functor = psl2bed_unsorted_line_functor;
        psl2bed_unsorted_stage.src = 0;
        psl2bed_unsorted_stage.dest = 1;

        bed_unsorted2stdout_stage.pipeset = p;
        bed_unsorted2stdout_stage.line_functor = NULL;
        bed_unsorted2stdout_stage.src = 1;
        bed_unsorted2stdout_stage.dest = -1; /* dest BED is stdout */
    }
    else if (c2b_globals.output_format_idx == BED_FORMAT) {
        cat2psl_stage.pipeset = p;
        cat2psl_stage.line_functor = NULL;
        cat2psl_stage.src = -1; /* src is really stdin */
        cat2psl_stage.dest = 0;
        
        psl2bed_unsorted_stage.pipeset = p;
        psl2bed_unsorted_stage.line_functor = psl2bed_unsorted_line_functor;
        psl2bed_unsorted_stage.src = 0;
        psl2bed_unsorted_stage.dest = 1;
        
        bed_unsorted2bed_sorted_stage.pipeset = p;
        bed_unsorted2bed_sorted_stage.line_functor = NULL;
        bed_unsorted2bed_sorted_stage.src = 1;
        bed_unsorted2bed_sorted_stage.dest = 2;

        bed_sorted2stdout_stage.pipeset = p;
        bed_sorted2stdout_stage.line_functor = NULL;
        bed_sorted2stdout_stage.src = 2;
        bed_sorted2stdout_stage.dest = -1; /* dest BED is stdout */
    }
    else if (c2b_globals.output_format_idx == STARCH_FORMAT) {
        cat2psl_stage.pipeset = p;
        cat2psl_stage.line_functor = NULL;
        cat2psl_stage.src = -1; /* src is really stdin */
        cat2psl_stage.dest = 0;
        
        psl2bed_unsorted_stage.pipeset = p;
        psl2bed_unsorted_stage.line_functor = psl2bed_unsorted_line_functor;
        psl2bed_unsorted_stage.src = 0;
        psl2bed_unsorted_stage.dest = 1;

        bed_unsorted2bed_sorted_stage.pipeset = p;
        bed_unsorted2bed_sorted_stage.line_functor = NULL;
        bed_unsorted2bed_sorted_stage.src = 1;
        bed_unsorted2bed_sorted_stage.dest = 2;

        bed_sorted2starch_stage.pipeset = p;
        bed_sorted2starch_stage.line_functor = NULL;
        bed_sorted2starch_stage.src = 2;
        bed_sorted2starch_stage.dest = 3;

        starch2stdout_stage.pipeset = p;
        starch2stdout_stage.line_functor = NULL;
        starch2stdout_stage.src = 3;
        starch2stdout_stage.dest = -1; /* dest Starch is stdout */
    }
    else {
        fprintf(stderr, "Error: Unknown PSL conversion parameter combination\n");
        c2b_print_usage(stderr);
        exit(EXIT_FAILURE);
    }

    /*
       We open pid_t (process) instances to handle data in a specified order. 
    */

    c2b_cmd_cat_stdin(cat2psl_cmd);
#ifdef DEBUG
    fprintf(stderr, "Debug: c2b_cmd_cat_stdin: [%s]\n", cat2psl_cmd);
#endif
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunknown-pragmas"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"

    cat2psl_proc = c2b_popen4(cat2psl_cmd,
			      p->in[0],
			      p->out[0],
			      p->err[0],
			      POPEN4_FLAG_NONE);

#pragma GCC diagnostic pop

    if (c2b_globals.sort_flag) {
        c2b_cmd_sort_bed(bed_unsorted2bed_sorted_cmd);
#ifdef DEBUG
        fprintf(stderr, "Debug: c2b_cmd_sort_bed: [%s]\n", bed_unsorted2bed_sorted_cmd);
#endif
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunknown-pragmas"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
        bed_unsorted2bed_sorted_proc = c2b_popen4(bed_unsorted2bed_sorted_cmd,
                                                  p->in[2],
                                                  p->out[2],
                                                  p->err[2],
                                                  POPEN4_FLAG_NONE);
#pragma GCC diagnostic pop
    }

    if (c2b_globals.output_format_idx == STARCH_FORMAT) {
        c2b_cmd_starch_bed(bed_sorted2starch_cmd);
#ifdef DEBUG
        fprintf(stderr, "Debug: c2b_cmd_starch_bed: [%s]\n", bed_sorted2starch_cmd);
#endif
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunknown-pragmas"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
        bed_sorted2starch_proc = c2b_popen4(bed_sorted2starch_cmd,
                                            p->in[3],
                                            p->out[3],
                                            p->err[3],
                                            POPEN4_FLAG_NONE);
#pragma GCC diagnostic pop
    }

#ifdef DEBUG
    c2b_debug_pipeset(p, MAX_PIPES);
#endif

    /*
       Once we have the desired process instances, we create and join
       threads for their ordered execution.
    */

    if ((!c2b_globals.sort_flag) && (c2b_globals.output_format_idx == BED_FORMAT)) {
        pthread_create(&cat2psl_thread,
                       NULL,
                       c2b_read_bytes_from_stdin,
                       &cat2psl_stage);
        pthread_create(&psl2bed_unsorted_thread,
                       NULL,
                       c2b_process_intermediate_bytes_by_lines,
                       &psl2bed_unsorted_stage);
        pthread_create(&bed_unsorted2stdout_thread,
                       NULL,
                       c2b_write_in_bytes_to_stdout,
                       &bed_unsorted2stdout_stage);
    }
    else if (c2b_globals.output_format_idx == BED_FORMAT) {
        pthread_create(&cat2psl_thread,
                       NULL,
                       c2b_read_bytes_from_stdin,
                       &cat2psl_stage);
        pthread_create(&psl2bed_unsorted_thread,
                       NULL,
                       c2b_process_intermediate_bytes_by_lines,
                       &psl2bed_unsorted_stage);
        pthread_create(&bed_unsorted2bed_sorted_thread,
                       NULL,
                       c2b_write_in_bytes_to_in_process,
                       &bed_unsorted2bed_sorted_stage);
        pthread_create(&bed_sorted2stdout_thread,
                       NULL,
                       c2b_write_out_bytes_to_stdout,
                       &bed_sorted2stdout_stage);
    }
    else if (c2b_globals.output_format_idx == STARCH_FORMAT) {
        pthread_create(&cat2psl_thread,
                       NULL,
                       c2b_read_bytes_from_stdin,
                       &cat2psl_stage);
        pthread_create(&psl2bed_unsorted_thread,
                       NULL,
                       c2b_process_intermediate_bytes_by_lines,
                       &psl2bed_unsorted_stage);
        pthread_create(&bed_unsorted2bed_sorted_thread,
                       NULL,
                       c2b_write_in_bytes_to_in_process,
                       &bed_unsorted2bed_sorted_stage);
        pthread_create(&bed_sorted2starch_thread,
                       NULL,
                       c2b_write_out_bytes_to_in_process,
                       &bed_sorted2starch_stage);
        pthread_create(&starch2stdout_thread,
                       NULL,
                       c2b_write_out_bytes_to_stdout,
                       &starch2stdout_stage);
    }

    if ((!c2b_globals.sort_flag) && (c2b_globals.output_format_idx == BED_FORMAT)) {
        pthread_join(cat2psl_thread, (void **) NULL);
        pthread_join(psl2bed_unsorted_thread, (void **) NULL);
        pthread_join(bed_unsorted2stdout_thread, (void **) NULL);
    }
    else if (c2b_globals.output_format_idx == BED_FORMAT) {
        pthread_join(cat2psl_thread, (void **) NULL);
        pthread_join(psl2bed_unsorted_thread, (void **) NULL);
        pthread_join(bed_unsorted2bed_sorted_thread, (void **) NULL);
        pthread_join(bed_sorted2stdout_thread, (void **) NULL);
    }
    else if (c2b_globals.output_format_idx == STARCH_FORMAT) {
        pthread_join(cat2psl_thread, (void **) NULL);
        pthread_join(psl2bed_unsorted_thread, (void **) NULL);
        pthread_join(bed_unsorted2bed_sorted_thread, (void **) NULL);
        pthread_join(bed_sorted2starch_thread, (void **) NULL);
        pthread_join(starch2stdout_thread, (void **) NULL);
    }

#ifdef DEBUG
    fprintf(stderr, "--- c2b_init_psl_conversion() - exit  ---\n");
#endif
}

static void
c2b_init_sam_conversion(c2b_pipeset_t *p)
{
#ifdef DEBUG
    fprintf(stderr, "--- c2b_init_sam_conversion() - enter ---\n");
#endif

    pthread_t cat2sam_thread; 
    pthread_t sam2bed_unsorted_thread; 
    pthread_t bed_unsorted2stdout_thread;
    pthread_t bed_unsorted2bed_sorted_thread;
    pthread_t bed_sorted2stdout_thread;
    pthread_t bed_sorted2starch_thread;
    pthread_t starch2stdout_thread;
    pid_t cat2sam_proc;
    pid_t bed_unsorted2bed_sorted_proc;
    pid_t bed_sorted2starch_proc;
    c2b_pipeline_stage_t cat2sam_stage;
    c2b_pipeline_stage_t sam2bed_unsorted_stage;
    c2b_pipeline_stage_t bed_unsorted2stdout_stage;
    c2b_pipeline_stage_t bed_unsorted2bed_sorted_stage;
    c2b_pipeline_stage_t bed_sorted2stdout_stage;
    c2b_pipeline_stage_t bed_sorted2starch_stage;
    c2b_pipeline_stage_t starch2stdout_stage;
    char cat2sam_cmd[MAX_LINE_LENGTH_VALUE] = {0};
    char bed_unsorted2bed_sorted_cmd[MAX_LINE_LENGTH_VALUE] = {0};
    char bed_sorted2starch_cmd[MAX_LINE_LENGTH_VALUE] = {0};
    void (*sam2bed_unsorted_line_functor)(char *, ssize_t *, char *, ssize_t) = NULL;

    sam2bed_unsorted_line_functor = (!c2b_globals.split_flag ?
                                     c2b_line_convert_sam_to_bed_unsorted_without_split_operation :
                                     c2b_line_convert_sam_to_bed_unsorted_with_split_operation);

    if ((!c2b_globals.sort_flag) && (c2b_globals.output_format_idx == BED_FORMAT)) {
        cat2sam_stage.pipeset = p;
        cat2sam_stage.line_functor = NULL;
        cat2sam_stage.src = -1; /* src is really stdin */
        cat2sam_stage.dest = 0;
        
        sam2bed_unsorted_stage.pipeset = p;
        sam2bed_unsorted_stage.line_functor = sam2bed_unsorted_line_functor;
        sam2bed_unsorted_stage.src = 0;
        sam2bed_unsorted_stage.dest = 1;

        bed_unsorted2stdout_stage.pipeset = p;
        bed_unsorted2stdout_stage.line_functor = NULL;
        bed_unsorted2stdout_stage.src = 1;
        bed_unsorted2stdout_stage.dest = -1; /* dest BED is stdout */
    }
    else if (c2b_globals.output_format_idx == BED_FORMAT) {
        cat2sam_stage.pipeset = p;
        cat2sam_stage.line_functor = NULL;
        cat2sam_stage.src = -1; /* src is really stdin */
        cat2sam_stage.dest = 0;
        
        sam2bed_unsorted_stage.pipeset = p;
        sam2bed_unsorted_stage.line_functor = sam2bed_unsorted_line_functor;
        sam2bed_unsorted_stage.src = 0;
        sam2bed_unsorted_stage.dest = 1;
        
        bed_unsorted2bed_sorted_stage.pipeset = p;
        bed_unsorted2bed_sorted_stage.line_functor = NULL;
        bed_unsorted2bed_sorted_stage.src = 1;
        bed_unsorted2bed_sorted_stage.dest = 2;

        bed_sorted2stdout_stage.pipeset = p;
        bed_sorted2stdout_stage.line_functor = NULL;
        bed_sorted2stdout_stage.src = 2;
        bed_sorted2stdout_stage.dest = -1; /* dest BED is stdout */
    }
    else if (c2b_globals.output_format_idx == STARCH_FORMAT) {
        cat2sam_stage.pipeset = p;
        cat2sam_stage.line_functor = NULL;
        cat2sam_stage.src = -1; /* src is really stdin */
        cat2sam_stage.dest = 0;
        
        sam2bed_unsorted_stage.pipeset = p;
        sam2bed_unsorted_stage.line_functor = sam2bed_unsorted_line_functor;
        sam2bed_unsorted_stage.src = 0;
        sam2bed_unsorted_stage.dest = 1;

        bed_unsorted2bed_sorted_stage.pipeset = p;
        bed_unsorted2bed_sorted_stage.line_functor = NULL;
        bed_unsorted2bed_sorted_stage.src = 1;
        bed_unsorted2bed_sorted_stage.dest = 2;

        bed_sorted2starch_stage.pipeset = p;
        bed_sorted2starch_stage.line_functor = NULL;
        bed_sorted2starch_stage.src = 2;
        bed_sorted2starch_stage.dest = 3;

        starch2stdout_stage.pipeset = p;
        starch2stdout_stage.line_functor = NULL;
        starch2stdout_stage.src = 3;
        starch2stdout_stage.dest = -1; /* dest Starch is stdout */
    }
    else {
        fprintf(stderr, "Error: Unknown SAM conversion parameter combination\n");
        c2b_print_usage(stderr);
        exit(EXIT_FAILURE);
    }

    /*
       We open pid_t (process) instances to handle data in a specified order. 
    */

    c2b_cmd_cat_stdin(cat2sam_cmd);
#ifdef DEBUG
    fprintf(stderr, "Debug: c2b_cmd_cat_stdin: [%s]\n", cat2sam_cmd);
#endif
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunknown-pragmas"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"

    cat2sam_proc = c2b_popen4(cat2sam_cmd,
			      p->in[0],
			      p->out[0],
			      p->err[0],
			      POPEN4_FLAG_NONE);

#pragma GCC diagnostic pop

    if (c2b_globals.sort_flag) {
        c2b_cmd_sort_bed(bed_unsorted2bed_sorted_cmd);
#ifdef DEBUG
        fprintf(stderr, "Debug: c2b_cmd_sort_bed: [%s]\n", bed_unsorted2bed_sorted_cmd);
#endif
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunknown-pragmas"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
        bed_unsorted2bed_sorted_proc = c2b_popen4(bed_unsorted2bed_sorted_cmd,
                                                  p->in[2],
                                                  p->out[2],
                                                  p->err[2],
                                                  POPEN4_FLAG_NONE);
#pragma GCC diagnostic pop
    }

    if (c2b_globals.output_format_idx == STARCH_FORMAT) {
        c2b_cmd_starch_bed(bed_sorted2starch_cmd);
#ifdef DEBUG
        fprintf(stderr, "Debug: c2b_cmd_starch_bed: [%s]\n", bed_sorted2starch_cmd);
#endif
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunknown-pragmas"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
        bed_sorted2starch_proc = c2b_popen4(bed_sorted2starch_cmd,
                                            p->in[3],
                                            p->out[3],
                                            p->err[3],
                                            POPEN4_FLAG_NONE);
#pragma GCC diagnostic pop
    }

#ifdef DEBUG
    c2b_debug_pipeset(p, MAX_PIPES);
#endif

    /*
       Once we have the desired process instances, we create and join
       threads for their ordered execution.
    */

    if ((!c2b_globals.sort_flag) && (c2b_globals.output_format_idx == BED_FORMAT)) {
        pthread_create(&cat2sam_thread,
                       NULL,
                       c2b_read_bytes_from_stdin,
                       &cat2sam_stage);
        pthread_create(&sam2bed_unsorted_thread,
                       NULL,
                       c2b_process_intermediate_bytes_by_lines,
                       &sam2bed_unsorted_stage);
        pthread_create(&bed_unsorted2stdout_thread,
                       NULL,
                       c2b_write_in_bytes_to_stdout,
                       &bed_unsorted2stdout_stage);
    }
    else if (c2b_globals.output_format_idx == BED_FORMAT) {
        pthread_create(&cat2sam_thread,
                       NULL,
                       c2b_read_bytes_from_stdin,
                       &cat2sam_stage);
        pthread_create(&sam2bed_unsorted_thread,
                       NULL,
                       c2b_process_intermediate_bytes_by_lines,
                       &sam2bed_unsorted_stage);
        pthread_create(&bed_unsorted2bed_sorted_thread,
                       NULL,
                       c2b_write_in_bytes_to_in_process,
                       &bed_unsorted2bed_sorted_stage);
        pthread_create(&bed_sorted2stdout_thread,
                       NULL,
                       c2b_write_out_bytes_to_stdout,
                       &bed_sorted2stdout_stage);
    }
    else if (c2b_globals.output_format_idx == STARCH_FORMAT) {
        pthread_create(&cat2sam_thread,
                       NULL,
                       c2b_read_bytes_from_stdin,
                       &cat2sam_stage);
        pthread_create(&sam2bed_unsorted_thread,
                       NULL,
                       c2b_process_intermediate_bytes_by_lines,
                       &sam2bed_unsorted_stage);
        pthread_create(&bed_unsorted2bed_sorted_thread,
                       NULL,
                       c2b_write_in_bytes_to_in_process,
                       &bed_unsorted2bed_sorted_stage);
        pthread_create(&bed_sorted2starch_thread,
                       NULL,
                       c2b_write_out_bytes_to_in_process,
                       &bed_sorted2starch_stage);
        pthread_create(&starch2stdout_thread,
                       NULL,
                       c2b_write_out_bytes_to_stdout,
                       &starch2stdout_stage);
    }

    if ((!c2b_globals.sort_flag) && (c2b_globals.output_format_idx == BED_FORMAT)) {
        pthread_join(cat2sam_thread, (void **) NULL);
        pthread_join(sam2bed_unsorted_thread, (void **) NULL);
        pthread_join(bed_unsorted2stdout_thread, (void **) NULL);
    }
    else if (c2b_globals.output_format_idx == BED_FORMAT) {
        pthread_join(cat2sam_thread, (void **) NULL);
        pthread_join(sam2bed_unsorted_thread, (void **) NULL);
        pthread_join(bed_unsorted2bed_sorted_thread, (void **) NULL);
        pthread_join(bed_sorted2stdout_thread, (void **) NULL);
    }
    else if (c2b_globals.output_format_idx == STARCH_FORMAT) {
        pthread_join(cat2sam_thread, (void **) NULL);
        pthread_join(sam2bed_unsorted_thread, (void **) NULL);
        pthread_join(bed_unsorted2bed_sorted_thread, (void **) NULL);
        pthread_join(bed_sorted2starch_thread, (void **) NULL);
        pthread_join(starch2stdout_thread, (void **) NULL);
    }

#ifdef DEBUG
    fprintf(stderr, "--- c2b_init_sam_conversion() - exit  ---\n");
#endif
}

static inline void
c2b_cmd_cat_stdin(char *cmd)
{
    const char *cat_args = " - ";
    
    /* /path/to/cat - */
    memcpy(cmd,
           c2b_globals.cat_path,
           strlen(c2b_globals.cat_path));
    memcpy(cmd + strlen(c2b_globals.cat_path),
           cat_args,
           strlen(cat_args));
}

static inline void
c2b_cmd_bam_to_sam(char *cmd)
{
    const char *bam2sam_args = " view -h -";

    /* /path/to/samtools view -h - */
    memcpy(cmd, 
           c2b_globals.samtools_path, 
           strlen(c2b_globals.samtools_path));
    memcpy(cmd + strlen(c2b_globals.samtools_path), 
           bam2sam_args, 
           strlen(bam2sam_args));
}

static inline void
c2b_cmd_sort_bed(char *cmd)
{
    char sort_bed_args[MAX_LINE_LENGTH_VALUE] = {0};

    /* /path/to/sort-bed [--max-mem <val>] [--tmpdir <path>] - */
    if (c2b_globals.max_mem_value) {
        memcpy(sort_bed_args + strlen(sort_bed_args),
               sort_bed_max_mem_arg, 
               strlen(sort_bed_max_mem_arg));
        memcpy(sort_bed_args + strlen(sort_bed_args), 
               c2b_globals.max_mem_value, 
               strlen(c2b_globals.max_mem_value));
    }
    else {
        memcpy(sort_bed_args, 
               sort_bed_max_mem_default_arg, 
               strlen(sort_bed_max_mem_default_arg));
    }
    if (c2b_globals.sort_tmpdir_path) {
        memcpy(sort_bed_args + strlen(sort_bed_args),
               sort_bed_tmpdir_arg,
               strlen(sort_bed_tmpdir_arg));
        memcpy(sort_bed_args + strlen(sort_bed_args),
               c2b_globals.sort_tmpdir_path,
               strlen(c2b_globals.sort_tmpdir_path));
    }
    memcpy(sort_bed_args + strlen(sort_bed_args),
           sort_bed_stdin,
           strlen(sort_bed_stdin));

    /* cmd */
    memcpy(cmd, 
           c2b_globals.sort_bed_path, 
           strlen(c2b_globals.sort_bed_path));
    memcpy(cmd + strlen(c2b_globals.sort_bed_path), 
           sort_bed_args, 
           strlen(sort_bed_args));
}

static inline void
c2b_cmd_starch_bed(char *cmd) 
{
    char starch_args[MAX_LINE_LENGTH_VALUE] = {0};

    /* /path/to/starch [--bzip2 | --gzip] [--note="xyz..."] - */
    if (c2b_globals.starch_bzip2_flag) {
        memcpy(starch_args,
               starch_bzip2_arg,
               strlen(starch_bzip2_arg));
    }
    else if (c2b_globals.starch_gzip_flag) {
        memcpy(starch_args + strlen(starch_args),
               starch_gzip_arg,
               strlen(starch_gzip_arg));
    }

#ifdef DEBUG
    fprintf(stderr, "Debug: c2b_globals.starch_bzip2_flag: [%d]\n", c2b_globals.starch_bzip2_flag);
    fprintf(stderr, "Debug: c2b_globals.starch_gzip_flag: [%d]\n", c2b_globals.starch_gzip_flag);
    fprintf(stderr, "Debug: starch_args: [%s]\n", starch_args);
#endif

    if (c2b_globals.starch_note) {
        memcpy(starch_args + strlen(starch_args),
               starch_note_prefix_arg,
               strlen(starch_note_prefix_arg));
        memcpy(starch_args + strlen(starch_args),
               c2b_globals.starch_note,
               strlen(c2b_globals.starch_note));
        memcpy(starch_args + strlen(starch_args),
               starch_note_suffix_arg,
               strlen(starch_note_suffix_arg));
    }
    memcpy(starch_args + strlen(starch_args),
           starch_stdin_arg,
           strlen(starch_stdin_arg));

#ifdef DEBUG
    fprintf(stderr, "Debug: starch_args: [%s]\n", starch_args);
#endif

    /* cmd */
    memcpy(cmd, 
           c2b_globals.starch_path, 
           strlen(c2b_globals.starch_path));
    memcpy(cmd + strlen(c2b_globals.starch_path), 
           starch_args, 
           strlen(starch_args));
}

static void
c2b_line_convert_gtf_to_bed_unsorted(char *dest, ssize_t *dest_size, char *src, ssize_t src_size)
{
    ssize_t gtf_field_offsets[MAX_FIELD_LENGTH_VALUE] = {-1};
    int gtf_field_idx = 0;
    ssize_t current_src_posn = -1;

    while (++current_src_posn < src_size) {
        if ((src[current_src_posn] == c2b_tab_delim) || (src[current_src_posn] == c2b_line_delim)) {
            gtf_field_offsets[gtf_field_idx++] = current_src_posn;
        }
    }
    gtf_field_offsets[gtf_field_idx] = src_size;

    /* 
       If number of fields is not in bounds, we may need to exit early
    */

    if (((gtf_field_idx + 1) < c2b_gtf_field_min) || ((gtf_field_idx + 1) > c2b_gtf_field_max)) {
        if (gtf_field_idx == 0) {
            if (src[0] == c2b_gtf_comment) {
                if (!c2b_globals.keep_header_flag) {
                    return;
                }
                else {
                    /* copy header line to destination stream buffer */
                    char src_header_line_str[MAX_LINE_LENGTH_VALUE] = {0};
                    char dest_header_line_str[MAX_LINE_LENGTH_VALUE] = {0};
                    memcpy(src_header_line_str, src, src_size);
                    sprintf(dest_header_line_str, "%s\t%u\t%u\t%s\n", c2b_header_chr_name, c2b_globals.header_line_idx, (c2b_globals.header_line_idx + 1), src_header_line_str);
                    memcpy(dest + *dest_size, dest_header_line_str, strlen(dest_header_line_str));
                    *dest_size += strlen(dest_header_line_str);
                    c2b_globals.header_line_idx++;
                    return;
                }
            }
            else {
                return;
            }
        }
        else {
            fprintf(stderr, "Error: Invalid field count (%d) -- input file may not match input format\n", gtf_field_idx);
            c2b_print_usage(stderr);
            exit(EXIT_FAILURE);
        }
    }

    /* 0 - seqname */
    char seqname_str[MAX_FIELD_LENGTH_VALUE] = {0};
    ssize_t seqname_size = gtf_field_offsets[0] - 1;
    memcpy(seqname_str, src, seqname_size);

    /* 1 - source */
    char source_str[MAX_FIELD_LENGTH_VALUE] = {0};
    ssize_t source_size = gtf_field_offsets[1] - gtf_field_offsets[0] - 1;
    memcpy(source_str, src + gtf_field_offsets[0] + 1, source_size);

    /* 2 - feature */
    char feature_str[MAX_FIELD_LENGTH_VALUE] = {0};
    ssize_t feature_size = gtf_field_offsets[2] - gtf_field_offsets[1] - 1;
    memcpy(feature_str, src + gtf_field_offsets[1] + 1, feature_size);

    /* 3 - start */
    char start_str[MAX_FIELD_LENGTH_VALUE] = {0};
    ssize_t start_size = gtf_field_offsets[3] - gtf_field_offsets[2] - 1;
    memcpy(start_str, src + gtf_field_offsets[2] + 1, start_size);
    unsigned long long int start_val = strtoull(start_str, NULL, 10);

    /* 4 - end */
    char end_str[MAX_FIELD_LENGTH_VALUE] = {0};
    ssize_t end_size = gtf_field_offsets[4] - gtf_field_offsets[3] - 1;
    memcpy(end_str, src + gtf_field_offsets[3] + 1, end_size);
    unsigned long long int end_val = strtoull(end_str, NULL, 10);

    /* 5 - score */
    char score_str[MAX_FIELD_LENGTH_VALUE] = {0};
    ssize_t score_size = gtf_field_offsets[5] - gtf_field_offsets[4] - 1;
    memcpy(score_str, src + gtf_field_offsets[4] + 1, score_size);

    /* 6 - strand */
    char strand_str[MAX_FIELD_LENGTH_VALUE] = {0};
    ssize_t strand_size = gtf_field_offsets[6] - gtf_field_offsets[5] - 1;
    memcpy(strand_str, src + gtf_field_offsets[5] + 1, strand_size);

    /* 7 - frame */
    char frame_str[MAX_FIELD_LENGTH_VALUE] = {0};
    ssize_t frame_size = gtf_field_offsets[7] - gtf_field_offsets[6] - 1;
    memcpy(frame_str, src + gtf_field_offsets[6] + 1, frame_size);

    /* 8 - attributes */
    char attributes_str[MAX_FIELD_LENGTH_VALUE] = {0};
    ssize_t attributes_size = gtf_field_offsets[8] - gtf_field_offsets[7] - 1;
    memcpy(attributes_str, src + gtf_field_offsets[7] + 1, attributes_size);

    /* 9 - comments */
    char comments_str[MAX_FIELD_LENGTH_VALUE] = {0};
    ssize_t comments_size = 0;
    if (gtf_field_idx == 9) {
        comments_size = gtf_field_offsets[9] - gtf_field_offsets[8] - 1;
        memcpy(comments_str, src + gtf_field_offsets[8] + 1, comments_size);
    }

    c2b_gtf_t gtf;
    gtf.seqname = seqname_str;
    gtf.source = source_str;
    gtf.feature = feature_str;
    gtf.start = start_val;
    gtf.end = end_val;
    gtf.score = score_str;
    gtf.strand = strand_str;
    gtf.frame = frame_str;
    gtf.attributes = attributes_str;
    gtf.comments = comments_str;

    /* 
       Fix coordinate indexing, and (if needed) add attribute for zero-length record
    */

    if (gtf.start == gtf.end) {
        gtf.start -= 1;
        memcpy(attributes_str + strlen(attributes_str), 
               c2b_gtf_zero_length_insertion_attribute, 
               strlen(c2b_gtf_zero_length_insertion_attribute));
    }
    else {
        gtf.start -= 1;
    }

    /* 
       Parse ID value out from attributes string
    */

    char *attributes_copy = NULL;
    attributes_copy = malloc(strlen(attributes_str) + 1);
    if (!attributes_copy) {
        fprintf(stderr, "Error: Could not allocate space for GFF attributes copy\n");
        exit(EXIT_FAILURE);
    }
    memcpy(attributes_copy, attributes_str, strlen(attributes_str) + 1);
    const char *kv_tok;
    const char *gtf_id_prefix = "gene_id ";
    char *id_str;
    while((kv_tok = c2b_strsep(&attributes_copy, ";")) != NULL) {
        id_str = strstr(kv_tok, gtf_id_prefix);
        if (id_str) {
            /* we remove quotation marks around ID string value */
            memcpy(c2b_globals.gtf_id, kv_tok + strlen(gtf_id_prefix) + 1, strlen(kv_tok + strlen(gtf_id_prefix)) - 2);
        }
    }
    free(attributes_copy), attributes_copy = NULL;
    gtf.id = c2b_globals.gtf_id;

    /* 
       Convert GTF struct to BED string and copy it to destination
    */

    char dest_line_str[MAX_LINE_LENGTH_VALUE] = {0};
    c2b_line_convert_gtf_to_bed(gtf, dest_line_str);
    memcpy(dest + *dest_size, dest_line_str, strlen(dest_line_str));
    *dest_size += strlen(dest_line_str);
}

static inline void
c2b_line_convert_gtf_to_bed(c2b_gtf_t g, char *dest_line)
{
    /* 
       For GTF-formatted data, we use the mapping provided by BEDOPS convention described at:

       http://bedops.readthedocs.org/en/latest/content/reference/file-management/conversion/gtf2bed.html

       GTF field                 BED column index       BED field
       -------------------------------------------------------------------------
       seqname                   1                      chromosome
       start                     2                      start
       end                       3                      stop
       ID (via attributes)       4                      id
       score                     5                      score
       strand                    6                      strand

       The remaining GTF columns are mapped as-is, in same order, to adjacent BED columns:

       GTF field                 BED column index       BED field
       -------------------------------------------------------------------------
       source                    7                      -
       feature                   8                      -
       frame                     9                      -
       attributes                10                     -

       If present in the GTF2.2 input, the following column is also mapped:

       GTF field                 BED column index       BED field
       -------------------------------------------------------------------------
       comments                  11                     -
    */

    if (strlen(g.comments) == 0) {
        sprintf(dest_line,
                "%s\t"                          \
                "%" PRIu64 "\t"                 \
                "%" PRIu64 "\t"                 \
                "%s\t"                          \
                "%s\t"                          \
                "%s\t"                          \
                "%s\t"                          \
                "%s\t"                          \
                "%s\t"                          \
                "%s\n",
                g.seqname,
                g.start,
                g.end,
                g.id,
                g.score,
                g.strand,
                g.source,
                g.feature,
                g.frame,
                g.attributes);
    }
    else {
        sprintf(dest_line,
                "%s\t"                          \
                "%" PRIu64 "\t"                 \
                "%" PRIu64 "\t"                 \
                "%s\t"                          \
                "%s\t"                          \
                "%s\t"                          \
                "%s\t"                          \
                "%s\t"                          \
                "%s\t"                          \
                "%s\t"                          \
                "%s\n",
                g.seqname,
                g.start,
                g.end,
                g.id,
                g.score,
                g.strand,
                g.source,
                g.feature,
                g.frame,
                g.attributes,
                g.comments);
    }
}

static void
c2b_line_convert_gff_to_bed_unsorted(char *dest, ssize_t *dest_size, char *src, ssize_t src_size)
{
    ssize_t gff_field_offsets[MAX_FIELD_LENGTH_VALUE] = {-1};
    int gff_field_idx = 0;
    ssize_t current_src_posn = -1;

    while (++current_src_posn < src_size) {
        if ((src[current_src_posn] == c2b_tab_delim) || (src[current_src_posn] == c2b_line_delim)) {
            gff_field_offsets[gff_field_idx++] = current_src_posn;
        }
    }
    gff_field_offsets[gff_field_idx] = src_size;

    /* 
       If number of fields is not in bounds, we may need to exit early
    */

    if (((gff_field_idx + 1) < c2b_gff_field_min) || ((gff_field_idx + 1) > c2b_gff_field_max)) {
        if (gff_field_idx == 0) {
            char non_interval_str[MAX_FIELD_LENGTH_VALUE] = {0};
            memcpy(non_interval_str, src, current_src_posn);
            if (strcmp(non_interval_str, c2b_gff_header) == 0) {
                if (!c2b_globals.keep_header_flag) {
                    return;
                }
                else {
                    /* copy header line to destination stream buffer */
                    char src_header_line_str[MAX_LINE_LENGTH_VALUE] = {0};
                    char dest_header_line_str[MAX_LINE_LENGTH_VALUE] = {0};
                    memcpy(src_header_line_str, src, src_size);
                    sprintf(dest_header_line_str, "%s\t%u\t%u\t%s\n", c2b_header_chr_name, c2b_globals.header_line_idx, (c2b_globals.header_line_idx + 1), src_header_line_str);
                    memcpy(dest + *dest_size, dest_header_line_str, strlen(dest_header_line_str));
                    *dest_size += strlen(dest_header_line_str);
                    c2b_globals.header_line_idx++;
                    return;                    
                }
            }
            else if (strcmp(non_interval_str, c2b_gff_fasta) == 0) {
                return;
            }
            else {
                return;
            }
        }
        else {
            fprintf(stderr, "Error: Invalid field count (%d) -- input file may not match input format\n", gff_field_idx);
            c2b_print_usage(stderr);
            exit(EXIT_FAILURE);
        }
    }

    /* 0 - seqid */
    char seqid_str[MAX_FIELD_LENGTH_VALUE] = {0};
    ssize_t seqid_size = gff_field_offsets[0] - 1;
    memcpy(seqid_str, src, seqid_size);

    /* 1 - source */
    char source_str[MAX_FIELD_LENGTH_VALUE] = {0};
    ssize_t source_size = gff_field_offsets[1] - gff_field_offsets[0] - 1;
    memcpy(source_str, src + gff_field_offsets[0] + 1, source_size);

    /* 2 - type */
    char type_str[MAX_FIELD_LENGTH_VALUE] = {0};
    ssize_t type_size = gff_field_offsets[2] - gff_field_offsets[1] - 1;
    memcpy(type_str, src + gff_field_offsets[1] + 1, type_size);

    /* 3 - start */
    char start_str[MAX_FIELD_LENGTH_VALUE] = {0};
    ssize_t start_size = gff_field_offsets[3] - gff_field_offsets[2] - 1;
    memcpy(start_str, src + gff_field_offsets[2] + 1, start_size);
    unsigned long long int start_val = strtoull(start_str, NULL, 10);

    /* 4 - end */
    char end_str[MAX_FIELD_LENGTH_VALUE] = {0};
    ssize_t end_size = gff_field_offsets[4] - gff_field_offsets[3] - 1;
    memcpy(end_str, src + gff_field_offsets[3] + 1, end_size);
    unsigned long long int end_val = strtoull(end_str, NULL, 10);

    /* 5 - score */
    char score_str[MAX_FIELD_LENGTH_VALUE] = {0};
    ssize_t score_size = gff_field_offsets[5] - gff_field_offsets[4] - 1;
    memcpy(score_str, src + gff_field_offsets[4] + 1, score_size);

    /* 6 - strand */
    char strand_str[MAX_FIELD_LENGTH_VALUE] = {0};
    ssize_t strand_size = gff_field_offsets[6] - gff_field_offsets[5] - 1;
    memcpy(strand_str, src + gff_field_offsets[5] + 1, strand_size);

    /* 7 - phase */
    char phase_str[MAX_FIELD_LENGTH_VALUE] = {0};
    ssize_t phase_size = gff_field_offsets[7] - gff_field_offsets[6] - 1;
    memcpy(phase_str, src + gff_field_offsets[6] + 1, phase_size);

    /* 8 - attributes */
    char attributes_str[MAX_FIELD_LENGTH_VALUE] = {0};
    ssize_t attributes_size = gff_field_offsets[8] - gff_field_offsets[7] - 1;
    memcpy(attributes_str, src + gff_field_offsets[7] + 1, attributes_size);

    c2b_gff_t gff;
    gff.seqid = seqid_str;
    gff.source = source_str;
    gff.type = type_str;
    gff.start = start_val;
    gff.end = end_val;
    gff.score = score_str;
    gff.strand = strand_str;
    gff.phase = phase_str;
    gff.attributes = attributes_str;

    /* 
       Fix coordinate indexing, and (if needed) add attribute for zero-length record
    */

    if (gff.start == gff.end) {
        gff.start -= 1;
        memcpy(attributes_str + strlen(attributes_str), 
               c2b_gff_zero_length_insertion_attribute, 
               strlen(c2b_gff_zero_length_insertion_attribute));
    }
    else {
        gff.start -= 1;
    }

    /* 
       Parse ID value out from attributes string
    */

    char *attributes_copy = NULL;
    attributes_copy = malloc(strlen(attributes_str) + 1);
    if (!attributes_copy) {
        fprintf(stderr, "Error: Could not allocate space for GFF attributes copy\n");
        exit(EXIT_FAILURE);
    }
    memcpy(attributes_copy, attributes_str, strlen(attributes_str) + 1);
    const char *kv_tok;
    const char *gff_id_prefix = "ID=";
    char *id_str;
    while((kv_tok = c2b_strsep(&attributes_copy, ";")) != NULL) {
        id_str = strstr(kv_tok, gff_id_prefix);
        if (id_str) {
            memcpy(c2b_globals.gff_id, kv_tok + strlen(gff_id_prefix), strlen(kv_tok + strlen(gff_id_prefix)) + 1);
        }
    }
    free(attributes_copy), attributes_copy = NULL;
    gff.id = c2b_globals.gff_id;

    /* 
       Convert GFF struct to BED string and copy it to destination
    */

    char dest_line_str[MAX_LINE_LENGTH_VALUE] = {0};
    c2b_line_convert_gff_to_bed(gff, dest_line_str);
    memcpy(dest + *dest_size, dest_line_str, strlen(dest_line_str));
    *dest_size += strlen(dest_line_str);
}

static inline void
c2b_line_convert_gff_to_bed(c2b_gff_t g, char *dest_line)
{
    /* 
       For GFF-formatted data, we use the mapping provided by BEDOPS convention described at:

       http://bedops.readthedocs.org/en/latest/content/reference/file-management/conversion/gff2bed.html

       GFF field                 BED column index       BED field
       -------------------------------------------------------------------------
       seqid                     1                      chromosome
       start                     2                      start
       end                       3                      stop
       ID (via attributes)       4                      id
       score                     5                      score
       strand                    6                      strand

       The remaining GFF columns are mapped as-is, in same order, to adjacent BED columns:

       GFF field                 BED column index       BED field
       -------------------------------------------------------------------------
       source                    7                      -
       type                      8                      -
       phase                     9                      -
       attributes                10                     -
    */

    sprintf(dest_line,
            "%s\t"                              \
            "%" PRIu64 "\t"                     \
            "%" PRIu64 "\t"                     \
            "%s\t"                              \
            "%s\t"                              \
            "%s\t"                              \
            "%s\t"                              \
            "%s\t"                              \
            "%s\t"                              \
            "%s\n",
            g.seqid,
            g.start,
            g.end,
            g.id,
            g.score,
            g.strand,
            g.source,
            g.type,
            g.phase,
            g.attributes);
}

static void
c2b_line_convert_psl_to_bed_unsorted(char *dest, ssize_t *dest_size, char *src, ssize_t src_size)
{
    ssize_t psl_field_offsets[MAX_FIELD_LENGTH_VALUE] = {-1};
    int psl_field_idx = 0;
    ssize_t current_src_posn = -1;

    while (++current_src_posn < src_size) {
        if ((src[current_src_posn] == c2b_tab_delim) || (src[current_src_posn] == c2b_line_delim)) {
            psl_field_offsets[psl_field_idx++] = current_src_posn;
        }
    }
    psl_field_offsets[psl_field_idx] = src_size;

    /* 
       If number of fields is not in bounds, we may need to exit early
    */

    if (((psl_field_idx + 1) < c2b_psl_field_min) || ((psl_field_idx + 1) > c2b_psl_field_max)) {
        if ((psl_field_idx == 0) || (psl_field_idx == 17)) {
            if ((c2b_globals.headered_flag) && (c2b_globals.keep_header_flag) && (c2b_globals.header_line_idx <= 5)) {
                /* copy header line to destination stream buffer */
                char src_header_line_str[MAX_LINE_LENGTH_VALUE] = {0};
                char dest_header_line_str[MAX_LINE_LENGTH_VALUE] = {0};
                memcpy(src_header_line_str, src, src_size);
                sprintf(dest_header_line_str, "%s\t%u\t%u\t%s\n", c2b_header_chr_name, c2b_globals.header_line_idx, (c2b_globals.header_line_idx + 1), src_header_line_str);
                memcpy(dest + *dest_size, dest_header_line_str, strlen(dest_header_line_str));
                *dest_size += strlen(dest_header_line_str);
                c2b_globals.header_line_idx++;
                return;                    
            }
            else if ((c2b_globals.headered_flag) && (c2b_globals.header_line_idx <= 5)) {
                c2b_globals.header_line_idx++;
                return;
            }
            else {
                fprintf(stderr, "Error: Possible corrupt input on line %u -- if input is headered, use the --headered option\n", c2b_globals.header_line_idx);
                c2b_print_usage(stderr);
                exit(EXIT_FAILURE);
            }
        }
        else {
            fprintf(stderr, "Error: Invalid field count (%d) -- input file may not match input format\n", psl_field_idx);
            c2b_print_usage(stderr);
            exit(EXIT_FAILURE);
        }
    }

    /* 0 - matches */
    char matches_str[MAX_FIELD_LENGTH_VALUE] = {0};
    ssize_t matches_size = psl_field_offsets[0] - 1;
    memcpy(matches_str, src, matches_size);
    unsigned long long int matches_val = strtoull(matches_str, NULL, 10);

    /* 1 - misMatches */
    char misMatches_str[MAX_FIELD_LENGTH_VALUE] = {0};
    ssize_t misMatches_size = psl_field_offsets[1] - psl_field_offsets[0] - 1;
    memcpy(misMatches_str, src + psl_field_offsets[0] + 1, misMatches_size);
    unsigned long long int misMatches_val = strtoull(misMatches_str, NULL, 10);

    /* 2 - repMatches */
    char repMatches_str[MAX_FIELD_LENGTH_VALUE] = {0};
    ssize_t repMatches_size = psl_field_offsets[2] - psl_field_offsets[1] - 1;
    memcpy(repMatches_str, src + psl_field_offsets[1] + 1, repMatches_size);
    unsigned long long int repMatches_val = strtoull(repMatches_str, NULL, 10);

    /* 3 - nCount */
    char nCount_str[MAX_FIELD_LENGTH_VALUE] = {0};
    ssize_t nCount_size = psl_field_offsets[3] - psl_field_offsets[2] - 1;
    memcpy(nCount_str, src + psl_field_offsets[2] + 1, nCount_size);
    unsigned long long int nCount_val = strtoull(nCount_str, NULL, 10);

    /* 4 - qNumInsert */
    char qNumInsert_str[MAX_FIELD_LENGTH_VALUE] = {0};
    ssize_t qNumInsert_size = psl_field_offsets[4] - psl_field_offsets[3] - 1;
    memcpy(qNumInsert_str, src + psl_field_offsets[3] + 1, qNumInsert_size);
    unsigned long long int qNumInsert_val = strtoull(qNumInsert_str, NULL, 10);

    /* 5 - qBaseInsert */
    char qBaseInsert_str[MAX_FIELD_LENGTH_VALUE] = {0};
    ssize_t qBaseInsert_size = psl_field_offsets[5] - psl_field_offsets[4] - 1;
    memcpy(qBaseInsert_str, src + psl_field_offsets[4] + 1, qBaseInsert_size);
    unsigned long long int qBaseInsert_val = strtoull(qBaseInsert_str, NULL, 10);

    /* 6 - tNumInsert */
    char tNumInsert_str[MAX_FIELD_LENGTH_VALUE] = {0};
    ssize_t tNumInsert_size = psl_field_offsets[6] - psl_field_offsets[5] - 1;
    memcpy(tNumInsert_str, src + psl_field_offsets[5] + 1, tNumInsert_size);
    unsigned long long int tNumInsert_val = strtoull(tNumInsert_str, NULL, 10);

    /* 7 - tBaseInsert */
    char tBaseInsert_str[MAX_FIELD_LENGTH_VALUE] = {0};
    ssize_t tBaseInsert_size = psl_field_offsets[7] - psl_field_offsets[6] - 1;
    memcpy(tBaseInsert_str, src + psl_field_offsets[6] + 1, tBaseInsert_size);
    unsigned long long int tBaseInsert_val = strtoull(tBaseInsert_str, NULL, 10);

    /* 8 - strand */
    char strand_str[MAX_FIELD_LENGTH_VALUE] = {0};
    ssize_t strand_size = psl_field_offsets[8] - psl_field_offsets[7] - 1;
    memcpy(strand_str, src + psl_field_offsets[7] + 1, strand_size);

    /* 9 - qName */
    char qName_str[MAX_FIELD_LENGTH_VALUE] = {0};
    ssize_t qName_size = psl_field_offsets[9] - psl_field_offsets[8] - 1;
    memcpy(qName_str, src + psl_field_offsets[8] + 1, qName_size);

    /* 10 - qSize */
    char qSize_str[MAX_FIELD_LENGTH_VALUE] = {0};
    ssize_t qSize_size = psl_field_offsets[10] - psl_field_offsets[9] - 1;
    memcpy(qSize_str, src + psl_field_offsets[9] + 1, qSize_size);
    unsigned long long int qSize_val = strtoull(qSize_str, NULL, 10);

    /* 11 - qStart */
    char qStart_str[MAX_FIELD_LENGTH_VALUE] = {0};
    ssize_t qStart_size = psl_field_offsets[11] - psl_field_offsets[10] - 1;
    memcpy(qStart_str, src + psl_field_offsets[10] + 1, qStart_size);
    unsigned long long int qStart_val = strtoull(qStart_str, NULL, 10);

    /* 12 - qEnd */
    char qEnd_str[MAX_FIELD_LENGTH_VALUE] = {0};
    ssize_t qEnd_size = psl_field_offsets[12] - psl_field_offsets[11] - 1;
    memcpy(qEnd_str, src + psl_field_offsets[11] + 1, qEnd_size);
    unsigned long long int qEnd_val = strtoull(qEnd_str, NULL, 10);

    /* 13 - tName */
    char tName_str[MAX_FIELD_LENGTH_VALUE] = {0};
    ssize_t tName_size = psl_field_offsets[13] - psl_field_offsets[12] - 1;
    memcpy(tName_str, src + psl_field_offsets[12] + 1, tName_size);

    /* 14 - tSize */
    char tSize_str[MAX_FIELD_LENGTH_VALUE] = {0};
    ssize_t tSize_size = psl_field_offsets[14] - psl_field_offsets[13] - 1;
    memcpy(tSize_str, src + psl_field_offsets[13] + 1, tSize_size);
    unsigned long long int tSize_val = strtoull(tSize_str, NULL, 10);

    /* 15 - tStart */
    char tStart_str[MAX_FIELD_LENGTH_VALUE] = {0};
    ssize_t tStart_size = psl_field_offsets[15] - psl_field_offsets[14] - 1;
    memcpy(tStart_str, src + psl_field_offsets[14] + 1, tStart_size);
    unsigned long long int tStart_val = strtoull(tStart_str, NULL, 10);

    /* 16 - tEnd */
    char tEnd_str[MAX_FIELD_LENGTH_VALUE] = {0};
    ssize_t tEnd_size = psl_field_offsets[16] - psl_field_offsets[15] - 1;
    memcpy(tEnd_str, src + psl_field_offsets[15] + 1, tEnd_size);
    unsigned long long int tEnd_val = strtoull(tEnd_str, NULL, 10);

    /* 17 - blockCount */
    char blockCount_str[MAX_FIELD_LENGTH_VALUE] = {0};
    ssize_t blockCount_size = psl_field_offsets[17] - psl_field_offsets[16] - 1;
    memcpy(blockCount_str, src + psl_field_offsets[16] + 1, blockCount_size);
    unsigned long long int blockCount_val = strtoull(blockCount_str, NULL, 10);

    /* 18 - blockSizes */
    char blockSizes_str[MAX_FIELD_LENGTH_VALUE] = {0};
    ssize_t blockSizes_size = psl_field_offsets[18] - psl_field_offsets[17] - 1;
    memcpy(blockSizes_str, src + psl_field_offsets[17] + 1, blockSizes_size);

    /* 19 - qStarts */
    char qStarts_str[MAX_FIELD_LENGTH_VALUE] = {0};
    ssize_t qStarts_size = psl_field_offsets[19] - psl_field_offsets[18] - 1;
    memcpy(qStarts_str, src + psl_field_offsets[18] + 1, qStarts_size);

    /* 20 - tStarts */
    char tStarts_str[MAX_FIELD_LENGTH_VALUE] = {0};
    ssize_t tStarts_size = psl_field_offsets[20] - psl_field_offsets[19] - 1;
    memcpy(tStarts_str, src + psl_field_offsets[19] + 1, tStarts_size);

    c2b_psl_t psl;
    psl.matches = matches_val;
    psl.misMatches = misMatches_val;
    psl.repMatches = repMatches_val;
    psl.nCount = nCount_val;
    psl.qNumInsert = qNumInsert_val;
    psl.qBaseInsert = qBaseInsert_val;
    psl.tNumInsert = tNumInsert_val;
    psl.tBaseInsert = tBaseInsert_val;
    psl.strand = strand_str;
    psl.qName = qName_str;
    psl.qSize = qSize_val;
    psl.qStart = qStart_val;
    psl.qEnd = qEnd_val;
    psl.tName = tName_str;
    psl.tSize = tSize_val;
    psl.tStart = tStart_val;
    psl.tEnd = tEnd_val;
    psl.blockCount = blockCount_val;
    psl.blockSizes = blockSizes_str;
    psl.qStarts = qStarts_str;
    psl.tStarts = tStarts_str;

    /* 
       Convert PSL struct to BED string and copy it to destination
    */

    char dest_line_str[MAX_LINE_LENGTH_VALUE] = {0};
    c2b_line_convert_psl_to_bed(psl, dest_line_str);
    memcpy(dest + *dest_size, dest_line_str, strlen(dest_line_str));
    *dest_size += strlen(dest_line_str);    
}

static inline void
c2b_line_convert_psl_to_bed(c2b_psl_t p, char *dest_line)
{
    /* 
       For PSL-formatted data, we use the mapping provided by BEDOPS convention described at:

       http://bedops.readthedocs.org/en/latest/content/reference/file-management/conversion/psl2bed.html

       PSL field                 BED column index       BED field
       -------------------------------------------------------------------------
       tName                     1                      chromosome
       tStart                    2                      start
       tEnd                      3                      stop
       qName                     4                      id
       qSize                     5                      score
       strand                    6                      strand

       The remaining PSL columns are mapped as-is, in same order, to adjacent BED columns:

       PSL field                 BED column index       BED field
       -------------------------------------------------------------------------
       matches                   7                      -
       misMatches                8                      -
       repMatches                9                      -
       nCount                    10                     -
       qNumInsert                11                     -
       qBaseInsert               12                     -
       tNumInsert                13                     -
       tBaseInsert               14                     -
       qStart                    15                     -
       qEnd                      16                     -
       tSize                     17                     -
       blockCount                18                     -
       blockSizes                19                     -
       qStarts                   20                     -
       tStarts                   21                     -
    */

    sprintf(dest_line,
            "%s\t"                              \
            "%" PRIu64 "\t"                     \
            "%" PRIu64 "\t"                     \
            "%s\t"                              \
            "%" PRIu64 "\t"                     \
            "%s\t"                              \
            "%" PRIu64 "\t"                     \
            "%" PRIu64 "\t"                     \
            "%" PRIu64 "\t"                     \
            "%" PRIu64 "\t"                     \
            "%" PRIu64 "\t"                     \
            "%" PRIu64 "\t"                     \
            "%" PRIu64 "\t"                     \
            "%" PRIu64 "\t"                     \
            "%" PRIu64 "\t"                     \
            "%" PRIu64 "\t"                     \
            "%" PRIu64 "\t"                     \
            "%" PRIu64 "\t"                     \
            "%s\t"                              \
            "%s\t"                              \
            "%s\n",
            p.tName,
            p.tStart,
            p.tEnd,
            p.qName,
            p.qSize,
            p.strand,
            p.matches,
            p.misMatches,
            p.repMatches,
            p.nCount,
            p.qNumInsert,
            p.qBaseInsert,
            p.tNumInsert,
            p.tBaseInsert,
            p.qStart,
            p.qEnd,
            p.tSize,
            p.blockCount,
            p.blockSizes,
            p.qStarts,
            p.tStarts);
}

static void
c2b_line_convert_sam_to_bed_unsorted_without_split_operation(char *dest, ssize_t *dest_size, char *src, ssize_t src_size)
{
    /* 
       Scan the src buffer (all src_size bytes of it) to build a list of tab delimiter 
       offsets. Then write content to the dest buffer, using offsets taken from the reordered 
       tab-offset list to grab fields in the correct order.
    */

    ssize_t sam_field_offsets[MAX_FIELD_LENGTH_VALUE] = {-1};
    int sam_field_idx = 0;
    ssize_t current_src_posn = -1;
    
    /* 
       Find offsets or process header line 
    */

    if (src[0] == c2b_sam_header_prefix) {
        if (!c2b_globals.keep_header_flag) {
            /* skip header line */
            return;
        }
        else {
            /* copy header line to destination stream buffer */
            char src_header_line_str[MAX_LINE_LENGTH_VALUE] = {0};
            char dest_header_line_str[MAX_LINE_LENGTH_VALUE] = {0};
            memcpy(src_header_line_str, src, src_size);
            sprintf(dest_header_line_str, "%s\t%u\t%u\t%s\n", c2b_header_chr_name, c2b_globals.header_line_idx, (c2b_globals.header_line_idx + 1), src_header_line_str);
            memcpy(dest + *dest_size, dest_header_line_str, strlen(dest_header_line_str));
            *dest_size += strlen(dest_header_line_str);
            c2b_globals.header_line_idx++;
            return;
        }
    }

    while (++current_src_posn < src_size) {
        if ((src[current_src_posn] == c2b_tab_delim) || (src[current_src_posn] == c2b_line_delim)) {
            sam_field_offsets[sam_field_idx++] = current_src_posn;
        }
    }
    sam_field_offsets[sam_field_idx] = src_size;

    /* 
       If no more than one field is read in, then something went wrong
    */

    if (sam_field_idx == 0) {
        fprintf(stderr, "Error: Invalid field count (%d) -- input file may not match input format\n", sam_field_idx);
        c2b_print_usage(stderr);
        exit(EXIT_FAILURE);
    }

    /* 
       Firstly, is read mapped? If not, and c2b_globals.all_reads_flag is kFalse, we skip over this line
    */

    ssize_t flag_size = sam_field_offsets[1] - sam_field_offsets[0];
    char flag_src_str[MAX_FIELD_LENGTH_VALUE] = {0};
    memcpy(flag_src_str, src + sam_field_offsets[0] + 1, flag_size);
    int flag_val = (int) strtol(flag_src_str, NULL, 10);
    boolean is_mapped = (boolean) !(4 & flag_val);
    if ((!is_mapped) && (!c2b_globals.all_reads_flag)) 
        return;

    /* Field 1 - RNAME */
    if (is_mapped) {
        ssize_t rname_size = sam_field_offsets[2] - sam_field_offsets[1];
        memcpy(dest + *dest_size, src + sam_field_offsets[1] + 1, rname_size);
        *dest_size += rname_size;
    }
    else {
        char unmapped_read_chr_str[MAX_FIELD_LENGTH_VALUE] = {0};
        memcpy(unmapped_read_chr_str, c2b_unmapped_read_chr_name, strlen(c2b_unmapped_read_chr_name));
        unmapped_read_chr_str[strlen(c2b_unmapped_read_chr_name)] = '\t';
        memcpy(dest + *dest_size, unmapped_read_chr_str, strlen(unmapped_read_chr_str));
        *dest_size += strlen(unmapped_read_chr_str);
    }

    /* Field 2 - POS - 1 */
    ssize_t pos_size = sam_field_offsets[3] - sam_field_offsets[2];
    char pos_src_str[MAX_FIELD_LENGTH_VALUE] = {0};
    memcpy(pos_src_str, src + sam_field_offsets[2] + 1, pos_size - 1);
    unsigned long long int pos_val = strtoull(pos_src_str, NULL, 10);
    char start_str[MAX_FIELD_LENGTH_VALUE] = {0};
    sprintf(start_str, "%llu\t", (is_mapped) ? pos_val - 1 : 0);
    memcpy(dest + *dest_size, start_str, strlen(start_str));
    *dest_size += strlen(start_str);

    /* Field 3 - POS + length(CIGAR) - 1 */
    ssize_t cigar_size = sam_field_offsets[5] - sam_field_offsets[4];
    ssize_t cigar_length = 0;
    char stop_str[MAX_FIELD_LENGTH_VALUE] = {0};
    char cigar_str[MAX_FIELD_LENGTH_VALUE] = {0};
    memcpy(cigar_str, src + sam_field_offsets[4] + 1, cigar_size - 1);
    c2b_sam_cigar_str_to_ops(cigar_str);
    ssize_t block_idx = 0;
    for (block_idx = 0; block_idx < c2b_globals.cigar->length; ++block_idx) {
        cigar_length += c2b_globals.cigar->ops[block_idx].bases;
    }
    sprintf(stop_str, "%llu\t", (is_mapped) ? pos_val + cigar_length - 1 : 1);
    memcpy(dest + *dest_size, stop_str, strlen(stop_str));
    *dest_size += strlen(stop_str);

    /* Field 4 - QNAME */
    ssize_t qname_size = sam_field_offsets[0] + 1;
    memcpy(dest + *dest_size, src, qname_size);
    *dest_size += qname_size;

    /* Field 5 - FLAG */
    memcpy(dest + *dest_size, src + sam_field_offsets[0] + 1, flag_size);
    *dest_size += flag_size;

    /* Field 6 - 16 & FLAG */
    int strand_val = 0x10 & flag_val;
    char strand_str[MAX_STRAND_LENGTH_VALUE] = {0};
    sprintf(strand_str, "%c\t", (strand_val == 0x10) ? '-' : '+');
    memcpy(dest + *dest_size, strand_str, strlen(strand_str));
    *dest_size += strlen(strand_str);

    /* Field 7 - MAPQ */
    ssize_t mapq_size = sam_field_offsets[4] - sam_field_offsets[3];
    memcpy(dest + *dest_size, src + sam_field_offsets[3] + 1, mapq_size);
    *dest_size += mapq_size;

    /* Field 8 - CIGAR */
    memcpy(dest + *dest_size, src + sam_field_offsets[4] + 1, cigar_size);
    *dest_size += cigar_size;

    /* Field 9 - RNEXT */
    ssize_t rnext_size = sam_field_offsets[6] - sam_field_offsets[5];
    memcpy(dest + *dest_size, src + sam_field_offsets[5] + 1, rnext_size);
    *dest_size += rnext_size;

    /* Field 10 - PNEXT */
    ssize_t pnext_size = sam_field_offsets[7] - sam_field_offsets[6];
    memcpy(dest + *dest_size, src + sam_field_offsets[6] + 1, pnext_size);
    *dest_size += pnext_size;

    /* Field 11 - TLEN */
    ssize_t tlen_size = sam_field_offsets[8] - sam_field_offsets[7];
    memcpy(dest + *dest_size, src + sam_field_offsets[7] + 1, tlen_size);
    *dest_size += tlen_size;

    /* Field 12 - SEQ */
    ssize_t seq_size = sam_field_offsets[9] - sam_field_offsets[8];
    memcpy(dest + *dest_size, src + sam_field_offsets[8] + 1, seq_size);
    *dest_size += seq_size;

    /* Field 13 - QUAL */
    ssize_t qual_size = sam_field_offsets[10] - sam_field_offsets[9];
    memcpy(dest + *dest_size, src + sam_field_offsets[9] + 1, qual_size);
    *dest_size += qual_size;

    /* Field 14+ - Optional fields */
    if (sam_field_offsets[11] == -1)
        return;

    int field_idx;
    for (field_idx = 11; field_idx <= sam_field_idx; field_idx++) {
        ssize_t opt_size = sam_field_offsets[field_idx] - sam_field_offsets[field_idx - 1];
        memcpy(dest + *dest_size, src + sam_field_offsets[field_idx - 1] + 1, opt_size);
        *dest_size += opt_size;
    }
}

static void
c2b_line_convert_sam_to_bed_unsorted_with_split_operation(char *dest, ssize_t *dest_size, char *src, ssize_t src_size)
{
    /* 
       This functor is slightly more complex than c2b_line_convert_sam_to_bed_unsorted_without_split_operation() 
       as we need to build a list of tab delimiters, as before, but first read in the CIGAR string (6th field) and
       parse it for operation key-value pairs to loop through later on
    */

    ssize_t sam_field_offsets[MAX_FIELD_LENGTH_VALUE] = {-1};
    int sam_field_idx = 0;
    ssize_t current_src_posn = -1;

    /* 
       Find offsets or process header line 
    */

    if (src[0] == c2b_sam_header_prefix) {
        if (!c2b_globals.keep_header_flag) {
            /* skip header line */
            return;
        }
        else {
            /* copy header line to destination stream buffer */
            char src_header_line_str[MAX_LINE_LENGTH_VALUE] = {0};
            char dest_header_line_str[MAX_LINE_LENGTH_VALUE] = {0};
            memcpy(src_header_line_str, src, src_size);
            sprintf(dest_header_line_str, "%s\t%u\t%u\t%s\n", c2b_header_chr_name, c2b_globals.header_line_idx, (c2b_globals.header_line_idx + 1), src_header_line_str);
            memcpy(dest + *dest_size, dest_header_line_str, strlen(dest_header_line_str));
            *dest_size += strlen(dest_header_line_str);
            c2b_globals.header_line_idx++;
            return;
        }
    }

    while (++current_src_posn < src_size) {
        if ((src[current_src_posn] == c2b_tab_delim) || (src[current_src_posn] == c2b_line_delim)) {
            sam_field_offsets[sam_field_idx++] = current_src_posn;
        }
    }
    sam_field_offsets[sam_field_idx] = src_size;

    /* 
       If no more than one field is read in, then something went wrong
    */

    if (sam_field_idx == 0) {
        fprintf(stderr, "Error: Invalid field count (%d) -- input file may not match input format\n", sam_field_idx);
        c2b_print_usage(stderr);
        exit(EXIT_FAILURE);
    }

    /* 
       Translate CIGAR string to operations
    */

    ssize_t cigar_size = sam_field_offsets[5] - sam_field_offsets[4];
    char cigar_str[MAX_FIELD_LENGTH_VALUE] = {0};
    memcpy(cigar_str, src + sam_field_offsets[4] + 1, cigar_size - 1);
    c2b_sam_cigar_str_to_ops(cigar_str);
#ifdef DEBUG
    c2b_sam_debug_cigar_ops(c2b_globals.cigar);
#endif
    ssize_t cigar_length = 0;
    ssize_t op_idx = 0;
    for (op_idx = 0; op_idx < c2b_globals.cigar->length; ++op_idx) {
        cigar_length += c2b_globals.cigar->ops[op_idx].bases;
    }

    /* 
       Firstly, is the read mapped? If not, and c2b_globals.all_reads_flag is kFalse, we skip over this line
    */

    ssize_t flag_size = sam_field_offsets[1] - sam_field_offsets[0];
    char flag_src_str[MAX_FIELD_LENGTH_VALUE] = {0};
    memcpy(flag_src_str, src + sam_field_offsets[0] + 1, flag_size);
    int flag_val = (int) strtol(flag_src_str, NULL, 10);
    boolean is_mapped = (boolean) !(4 & flag_val);
    if ((!is_mapped) && (!c2b_globals.all_reads_flag)) 
        return;    

    /* 
       Secondly, we need to retrieve RNAME, POS, QNAME parameters
    */

    /* RNAME */
    char rname_str[MAX_FIELD_LENGTH_VALUE] = {0};
    if (is_mapped) {
        ssize_t rname_size = sam_field_offsets[2] - sam_field_offsets[1] - 1;
        memcpy(rname_str, src + sam_field_offsets[1] + 1, rname_size);
    }
    else {
        char unmapped_read_chr_str[MAX_FIELD_LENGTH_VALUE] = {0};
        memcpy(unmapped_read_chr_str, c2b_unmapped_read_chr_name, strlen(c2b_unmapped_read_chr_name));
        unmapped_read_chr_str[strlen(c2b_unmapped_read_chr_name)] = '\t';
        memcpy(rname_str, unmapped_read_chr_str, strlen(unmapped_read_chr_str));
    }

    /* POS */
    ssize_t pos_size = sam_field_offsets[3] - sam_field_offsets[2];
    char pos_src_str[MAX_FIELD_LENGTH_VALUE] = {0};
    memcpy(pos_src_str, src + sam_field_offsets[2] + 1, pos_size - 1);
    unsigned long long int pos_val = strtoull(pos_src_str, NULL, 10);
    unsigned long long int start_val = pos_val - 1; /* remember, start = POS - 1 */
    unsigned long long int stop_val = start_val + cigar_length;

    /* QNAME */
    char qname_str[MAX_FIELD_LENGTH_VALUE] = {0};
    ssize_t qname_size = sam_field_offsets[0];
    memcpy(qname_str, src, qname_size);

    /* 16 & FLAG */
    int strand_val = 0x10 & flag_val;
    char strand_str[MAX_STRAND_LENGTH_VALUE] = {0};
    sprintf(strand_str, "%c", (strand_val == 0x10) ? '-' : '+');
    
    /* MAPQ */
    char mapq_str[MAX_FIELD_LENGTH_VALUE] = {0};
    ssize_t mapq_size = sam_field_offsets[4] - sam_field_offsets[3] - 1;
    memcpy(mapq_str, src + sam_field_offsets[3] + 1, mapq_size);
    
    /* RNEXT */
    char rnext_str[MAX_FIELD_LENGTH_VALUE] = {0};
    ssize_t rnext_size = sam_field_offsets[6] - sam_field_offsets[5] - 1;
    memcpy(rnext_str, src + sam_field_offsets[5] + 1, rnext_size);

    /* PNEXT */
    char pnext_str[MAX_FIELD_LENGTH_VALUE] = {0};
    ssize_t pnext_size = sam_field_offsets[7] - sam_field_offsets[6] - 1;
    memcpy(pnext_str, src + sam_field_offsets[6] + 1, pnext_size);

    /* TLEN */
    char tlen_str[MAX_FIELD_LENGTH_VALUE] = {0};
    ssize_t tlen_size = sam_field_offsets[8] - sam_field_offsets[7] - 1;
    memcpy(tlen_str, src + sam_field_offsets[7] + 1, tlen_size);

    /* SEQ */
    char seq_str[MAX_FIELD_LENGTH_VALUE] = {0};
    ssize_t seq_size = sam_field_offsets[9] - sam_field_offsets[8] - 1;
    memcpy(seq_str, src + sam_field_offsets[8] + 1, seq_size);

    /* QUAL */
    char qual_str[MAX_FIELD_LENGTH_VALUE] = {0};
    ssize_t qual_size = sam_field_offsets[10] - sam_field_offsets[9] - 1;
    memcpy(qual_str, src + sam_field_offsets[9] + 1, qual_size);

    /* Optional fields */
    char opt_str[MAX_FIELD_LENGTH_VALUE] = {0};
    if (sam_field_offsets[11] != -1) {
        for (int field_idx = 11; field_idx <= sam_field_idx; field_idx++) {
            ssize_t opt_size = sam_field_offsets[field_idx] - sam_field_offsets[field_idx - 1] - (field_idx == sam_field_idx ? 1 : 0);
            memcpy(opt_str + strlen(opt_str), src + sam_field_offsets[field_idx - 1] + 1, opt_size);
        }
    }

    /* 
       Loop through operations and process a line of input based on each operation and its associated value
    */

    ssize_t block_idx;
    char previous_op = default_cigar_op_operation;
    char modified_qname_str[MAX_FIELD_LENGTH_VALUE];

    c2b_sam_t sam;
    sam.rname = rname_str;
    sam.start = start_val;
    sam.stop = start_val;
    sam.qname = qname_str;
    sam.flag = flag_val;
    sam.strand = strand_str;
    sam.mapq = mapq_str;
    sam.cigar = cigar_str;
    sam.rnext = rnext_str;
    sam.pnext = pnext_str;
    sam.tlen = tlen_str;
    sam.seq = seq_str;
    sam.qual = qual_str;
    sam.opt = opt_str;

    char dest_line_str[MAX_LINE_LENGTH_VALUE] = {0};
    
    for (op_idx = 0, block_idx = 1; op_idx < c2b_globals.cigar->length; ++op_idx) {
        char current_op = c2b_globals.cigar->ops[op_idx].operation;
        unsigned int bases = c2b_globals.cigar->ops[op_idx].bases;
        switch (current_op) 
            {
            case 'M':
                sam.stop += bases;
                if ((previous_op == default_cigar_op_operation) || (previous_op == 'D') || (previous_op == 'N')) {
                    sprintf(modified_qname_str, "%s/%zu", qname_str, block_idx++);
                    sam.qname = modified_qname_str;
                    c2b_line_convert_sam_to_bed(sam, dest_line_str);
                    memcpy(dest + *dest_size, dest_line_str, strlen(dest_line_str));
                    *dest_size += strlen(dest_line_str);
                    sam.start = stop_val;
                }
                break;
            case 'N':
                sprintf(modified_qname_str, "%s/%zu", qname_str, block_idx++);
                sam.qname = modified_qname_str;
                c2b_line_convert_sam_to_bed(sam, dest_line_str);
                memcpy(dest + *dest_size, dest_line_str, strlen(dest_line_str));
                *dest_size += strlen(dest_line_str);
                sam.stop += bases;
                sam.start = sam.stop;
                break;
            case 'D':
                sam.stop += bases;
                sam.start = sam.stop;
                break;
            case 'H':
            case 'I':
            case 'P':
            case 'S':
                break;
            default:
                break;
            }
        previous_op = current_op;
    }

    /* 
       If the CIGAR string does not contain a split or deletion operation ('N', 'D') or the
       operations are unavailable ('*') then to quote Captain John O'Hagan, we don't enhance: we 
       just print the damn thing
    */

    if (block_idx == 1) {
        c2b_line_convert_sam_to_bed(sam, dest_line_str);
        memcpy(dest + *dest_size, dest_line_str, strlen(dest_line_str));
        *dest_size += strlen(dest_line_str);
    }
}

static inline void
c2b_line_convert_sam_to_bed(c2b_sam_t s, char *dest_line)
{
    /*
       For SAM-formatted data, we use the mapping provided by BEDOPS convention described at: 

       http://bedops.readthedocs.org/en/latest/content/reference/file-management/conversion/sam2bed.html

       SAM field                 BED column index       BED field
       -------------------------------------------------------------------------
       RNAME                     1                      chromosome
       POS - 1                   2                      start
       POS + length(CIGAR) - 1   3                      stop
       QNAME                     4                      id
       FLAG                      5                      score
       16 & FLAG                 6                      strand

       If NOT (4 & FLAG) is true, then the read is mapped.

       The remaining SAM columns are mapped as-is, in same order, to adjacent BED columns:

       SAM field                 BED column index       BED field
       -------------------------------------------------------------------------
       MAPQ                      7                      -
       CIGAR                     8                      -
       RNEXT                     9                      -
       PNEXT                     10                     -
       TLEN                      11                     -
       SEQ                       12                     -
       QUAL                      13                     -

       If present:

       SAM field                 BED column index       BED field
       -------------------------------------------------------------------------
       Alignment fields          14+                    -
    */

    if (strlen(s.opt)) {
        sprintf(dest_line,
                "%s\t"                          \
                "%" PRIu64 "\t"                 \
                "%" PRIu64 "\t"                 \
                "%s\t"                          \
                "%d\t"                          \
                "%s\t"                          \
                "%s\t"                          \
                "%s\t"                          \
                "%s\t"                          \
                "%s\t"                          \
                "%s\t"                          \
                "%s\t"                          \
                "%s\t"                          \
                "%s\n",
                s.rname,
                s.start,
                s.stop,
                s.qname,
                s.flag,
                s.strand,
                s.mapq,
                s.cigar,
                s.rnext,
                s.pnext,
                s.tlen,
                s.seq,
                s.qual,
                s.opt);
    } 
    else {
        sprintf(dest_line,
                "%s\t"                          \
                "%" PRIu64 "\t"                 \
                "%" PRIu64 "\t"                 \
                "%s\t"                          \
                "%s\t"                          \
                "%s\t"                          \
                "%s\t"                          \
                "%s\t"                          \
                "%s\t"                          \
                "%s\t"                          \
                "%s\t"                          \
                "%s\n",
                s.rname,
                s.start,
                s.stop,
                s.qname,
                s.strand,
                s.mapq,
                s.cigar,
                s.rnext,
                s.pnext,
                s.tlen,
                s.seq,
                s.qual);
    }
}

static void
c2b_sam_cigar_str_to_ops(char *s)
{
    size_t s_idx;
    size_t s_len = strlen(s);
    size_t bases_idx = 0;
    boolean bases_flag = kTrue;
    boolean operation_flag = kFalse;
    char curr_bases_field[MAX_OPERATION_FIELD_LENGTH_VALUE] = {0};
    char curr_char = default_cigar_op_operation;
    unsigned int curr_bases = 0;
    ssize_t op_idx = 0;

    for (s_idx = 0; s_idx < s_len; ++s_idx) {
        curr_char = s[s_idx];
        if (isdigit(curr_char)) {
            if (operation_flag) {
                c2b_globals.cigar->ops[op_idx].bases = curr_bases;
                op_idx++;
                operation_flag = kFalse;
                bases_flag = kTrue;
            }
            curr_bases_field[bases_idx++] = curr_char;
        }
        else {
            if (bases_flag) {
                curr_bases = atoi(curr_bases_field);
                bases_flag = kFalse;
                operation_flag = kTrue;
                bases_idx = 0;
                memset(curr_bases_field, 0, strlen(curr_bases_field));
            }
            c2b_globals.cigar->ops[op_idx].operation = curr_char;
            if (curr_char == '*') {
                break;
            }
        }
    }
    c2b_globals.cigar->ops[op_idx].bases = curr_bases;
    c2b_globals.cigar->length = op_idx + 1;
}

static void
c2b_sam_init_cigar_ops(c2b_cigar_t **c, const ssize_t size)
{
    *c = malloc(sizeof(c2b_cigar_t));
    if (!*c) {
        fprintf(stderr, "Error: Could not allocate space for CIGAR struct pointer\n");
        c2b_print_usage(stderr);
        exit(EXIT_FAILURE);
    }
    (*c)->ops = malloc(size * sizeof(c2b_cigar_op_t));
    if (!(*c)->ops) {
        fprintf(stderr, "Error: Could not allocate space for CIGAR struct operation pointer\n");
        c2b_print_usage(stderr);
        exit(EXIT_FAILURE);
    }
    (*c)->size = size;
    (*c)->length = 0;
    for (ssize_t idx = 0; idx < size; idx++) {
        (*c)->ops[idx].bases = default_cigar_op_bases;
        (*c)->ops[idx].operation = default_cigar_op_operation;
    }
}

/* 
   specifying special attribute for c2b_sam_debug_cigar_ops() to avoid: "warning: unused 
   function 'c2b_sam_debug_cigar_ops' [-Wunused-function]" message during compilation

   cf. http://gcc.gnu.org/onlinedocs/gcc-3.4.1/gcc/Function-Attributes.html#Function%20Attributes
*/
#if defined(__GNUC__)
static void c2b_sam_debug_cigar_ops() __attribute__ ((unused));
#endif

static void
c2b_sam_debug_cigar_ops(c2b_cigar_t *c)
{
    ssize_t idx = 0;
    ssize_t length = c->length;
    for (idx = 0; idx < length; ++idx) {
        fprintf(stderr, "\t-> c2b_sam_debug_cigar_ops - %zu [%03u, %c]\n", idx, c->ops[idx].bases, c->ops[idx].operation);
    }
}

static void
c2b_sam_delete_cigar_ops(c2b_cigar_t *c)
{
    if (c) {
        free(c->ops), c->ops = NULL, c->length = 0, c->size = 0;
        free(c), c = NULL;
    }
}

static void *
c2b_read_bytes_from_stdin(void *arg)
{
    c2b_pipeline_stage_t *stage = (c2b_pipeline_stage_t *) arg;
    c2b_pipeset_t *pipes = stage->pipeset;
    char buffer[MAX_LINE_LENGTH_VALUE];
    ssize_t bytes_read;

#ifdef DEBUG
    fprintf(stderr, "\t-> c2b_read_bytes_from_stdin | reading from fd     (%02d) | writing to fd     (%02d)\n", STDIN_FILENO, pipes->in[stage->dest][PIPE_WRITE]);
#endif

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-result"
    while ((bytes_read = read(STDIN_FILENO, buffer, MAX_LINE_LENGTH_VALUE)) > 0) {
        write(pipes->in[stage->dest][PIPE_WRITE], buffer, bytes_read);
    }
#pragma GCC diagnostic pop
    close(pipes->in[stage->dest][PIPE_WRITE]);

    pthread_exit(NULL);
}

static void *
c2b_process_intermediate_bytes_by_lines(void *arg)
{
    c2b_pipeline_stage_t *stage = (c2b_pipeline_stage_t *) arg;
    c2b_pipeset_t *pipes = stage->pipeset;
    char *src_buffer = NULL;
    ssize_t src_buffer_size = MAX_LINE_LENGTH_VALUE;
    ssize_t src_bytes_read = 0;
    ssize_t remainder_length = 0;
    ssize_t remainder_offset = 0;
    char line_delim = '\n';
    ssize_t lines_offset = 0;
    ssize_t start_offset = 0;
    ssize_t end_offset = 0;
    char *dest_buffer = NULL;
    ssize_t dest_buffer_size = MAX_LINE_LENGTH_VALUE * MAX_LINES_VALUE;
    ssize_t dest_bytes_written = 0;
    void (*line_functor)(char *, ssize_t *, char *, ssize_t) = stage->line_functor;

#ifdef DEBUG
    fprintf(stderr, "\t-> c2b_process_intermediate_bytes_by_lines | reading from fd  (%02d) | writing to fd  (%02d)\n", pipes->out[stage->src][PIPE_READ], pipes->in[stage->dest][PIPE_WRITE]);
#endif

    /* 
       We read from the src out pipe, then write to the dest in pipe 
    */
    
    src_buffer = malloc(src_buffer_size);
    if (!src_buffer) {
        fprintf(stderr, "Error: Could not allocate space for intermediate source buffer.\n");
        c2b_print_usage(stderr);
        exit(EXIT_FAILURE);
    }

    dest_buffer = malloc(dest_buffer_size);
    if (!dest_buffer) {
        fprintf(stderr, "Error: Could not allocate space for intermediate destination buffer.\n");
        c2b_print_usage(stderr);
        exit(EXIT_FAILURE);
    }

    while ((src_bytes_read = read(pipes->out[stage->src][PIPE_READ],
                                  src_buffer + remainder_length,
                                  src_buffer_size - remainder_length)) > 0) {

        /* 
           So here's what src_buffer looks like initially; basically, some stuff separated by
           newlines. The src_buffer will probably not terminate with a newline. So we first use 
           a custom memrchr() call to find the remainder_offset index value:
           
           src_buffer  [  .  .  .  \n  .  .  .  \n  .  .  .  \n  .  .  .  .  .  .  ]
           index        0 1 2 ...                            ^                    ^
                                                             |                    |
                                                             |   src_bytes_read --
                                                             | 
                                         remainder_offset --- 

           In other words, everything at and after index [remainder_offset + 1] to index
           [src_bytes_read - 1] is a remainder byte ("R"):

           src_buffer  [  .  .  .  \n  .  .  .  \n  .  .  .  \n R R R R R ]
           
           Assuming this failed:

           If remainder_offset is -1 and we have read src_buffer_size bytes, then we know there 
           are no newlines anywhere in the src_buffer and so we terminate early with an error state.
           This would suggest either src_buffer_size is too small to hold a whole intermediate 
           line or the input data is perhaps corrupt. Basically, something went wrong and we need 
           to investigate.
           
           Asumming this worked:
           
           We can now parse byte indices {[0 .. remainder_offset]} into lines, which are fed one
           by one to the line_functor. This functor parses out tab offsets and writes out a 
           reordered string based on the rules for the format (see BEDOPS docs for reordering 
           table).
           
           Finally, we write bytes from index [remainder_offset + 1] to [src_bytes_read - 1] 
           back to src_buffer. We are writing remainder_length bytes:
           
           new_remainder_length = current_src_bytes_read + old_remainder_length - new_remainder_offset
           
           Note that we can leave the rest of the buffer untouched:
           
           src_buffer  [ R R R R R \n  .  .  .  \n  .  .  .  \n  .  .  .  ]
           
           On the subsequent read, we want to read() into src_buffer at position remainder_length
           and read up to, at most, (src_buffer_size - remainder_length) bytes:
           
           read(byte_source, 
                src_buffer + remainder_length,
                src_buffer_size - remainder_length)

           Second and subsequent reads should reduce the maximum number of src_bytes_read from 
           src_buffer_size to something smaller.
        */

        c2b_memrchr_offset(&remainder_offset, src_buffer, src_buffer_size, src_bytes_read + remainder_length, line_delim);

        if (remainder_offset == -1) {
            if (src_bytes_read + remainder_length == src_buffer_size) {
                fprintf(stderr, "Error: Could not find newline in intermediate buffer; check input\n");
                c2b_print_usage(stderr);
                exit(EXIT_FAILURE);
            }
            remainder_offset = 0;
        }

        /* 
           We next want to process bytes from index [0] to index [remainder_offset - 1] for all
           lines contained within. We basically build a buffer containing all translated 
           lines to write downstream.
        */

        lines_offset = 0;
        start_offset = 0;
        dest_bytes_written = 0;
        while (lines_offset < remainder_offset) {
            if (src_buffer[lines_offset] == line_delim) {
                end_offset = lines_offset;
                /* for a given line from src, we write dest_bytes_written number of bytes to dest_buffer (plus written offset) */
                (*line_functor)(dest_buffer, &dest_bytes_written, src_buffer + start_offset, end_offset - start_offset);
                start_offset = end_offset + 1;
            }
            lines_offset++;            
        }
        
        /* 
           We have filled up dest_buffer with translated bytes (dest_bytes_written of them)
           and can now write() this buffer to the in-pipe of the destination stage
        */
        
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-result"
        write(pipes->in[stage->dest][PIPE_WRITE], dest_buffer, dest_bytes_written);
#pragma GCC diagnostic pop

        remainder_length = src_bytes_read + remainder_length - remainder_offset;
        memcpy(src_buffer, src_buffer + remainder_offset, remainder_length);
    }

    close(pipes->in[stage->dest][PIPE_WRITE]);

    if (src_buffer) 
        free(src_buffer), src_buffer = NULL;

    if (dest_buffer)
        free(dest_buffer), dest_buffer = NULL;

    pthread_exit(NULL);
}

static void *
c2b_write_in_bytes_to_in_process(void *arg)
{
    c2b_pipeline_stage_t *stage = (c2b_pipeline_stage_t *) arg;
    c2b_pipeset_t *pipes = stage->pipeset;
    char buffer[MAX_LINE_LENGTH_VALUE];
    ssize_t bytes_read;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-result"
    /* read buffer from p->in[1] and write buffer to p->in[2] */
    while ((bytes_read = read(pipes->in[stage->src][PIPE_READ], buffer, MAX_LINE_LENGTH_VALUE)) > 0) { 
        write(pipes->in[stage->dest][PIPE_WRITE], buffer, bytes_read);
    }
#pragma GCC diagnostic pop

    close(pipes->in[stage->dest][PIPE_WRITE]);

    pthread_exit(NULL);
}

static void *
c2b_write_out_bytes_to_in_process(void *arg)
{
    c2b_pipeline_stage_t *stage = (c2b_pipeline_stage_t *) arg;
    c2b_pipeset_t *pipes = stage->pipeset;
    char buffer[MAX_LINE_LENGTH_VALUE];
    ssize_t bytes_read;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-result"
    /* read buffer from p->out[1] and write buffer to p->in[2] */
    while ((bytes_read = read(pipes->out[stage->src][PIPE_READ], buffer, MAX_LINE_LENGTH_VALUE)) > 0) { 
        write(pipes->in[stage->dest][PIPE_WRITE], buffer, bytes_read);
    }
#pragma GCC diagnostic pop

    close(pipes->in[stage->dest][PIPE_WRITE]);

    pthread_exit(NULL);
}

static void *
c2b_write_in_bytes_to_stdout(void *arg)
{
    c2b_pipeline_stage_t *stage = (c2b_pipeline_stage_t *) arg;
    c2b_pipeset_t *pipes = stage->pipeset;
    char buffer[MAX_LINE_LENGTH_VALUE];
    ssize_t bytes_read;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-result"
    while ((bytes_read = read(pipes->in[stage->src][PIPE_READ], buffer, MAX_LINE_LENGTH_VALUE)) > 0) {
        write(STDOUT_FILENO, buffer, bytes_read);
    }
#pragma GCC diagnostic pop

    pthread_exit(NULL);
}

static void *
c2b_write_out_bytes_to_stdout(void *arg)
{
    c2b_pipeline_stage_t *stage = (c2b_pipeline_stage_t *) arg;
    c2b_pipeset_t *pipes = stage->pipeset;
    char buffer[MAX_LINE_LENGTH_VALUE];
    ssize_t bytes_read;

#ifdef DEBUG
    fprintf(stderr, "\t-> c2b_write_out_bytes_to_stdout | reading from fd  (%02d) | writing to fd  (%02d)\n", pipes->out[stage->src][PIPE_READ], STDOUT_FILENO);
#endif

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-result"
    while ((bytes_read = read(pipes->out[stage->src][PIPE_READ], buffer, MAX_LINE_LENGTH_VALUE)) > 0) {
        write(STDOUT_FILENO, buffer, bytes_read);
    }
#pragma GCC diagnostic pop

    pthread_exit(NULL);
}

static void
c2b_memrchr_offset(ssize_t *offset, char *buf, ssize_t buf_size, ssize_t len, char delim)
{
    ssize_t left = len;
    
    *offset = -1;
    if (len > buf_size)
        return;

    while (left > 0) {
        if (buf[left - 1] == delim) {
            *offset = left;
            return;
        }
        left--;
    }
}

static void
c2b_init_pipeset(c2b_pipeset_t *p, const size_t num)
{
#ifdef DEBUG
    fprintf(stderr, "--- c2b_init_pipeset() - enter ---\n");
#endif

    int **ins = NULL;
    int **outs = NULL;
    int **errs = NULL;
    size_t n;

    ins = malloc(num * sizeof(int *));
    outs = malloc(num * sizeof(int *));
    errs = malloc(num * sizeof(int *));
    if ((!ins) || (!outs) || (!errs)) {
        fprintf(stderr, "Error: Could not allocate space to temporary pipe arrays\n");
        c2b_print_usage(stderr);
        exit(EXIT_FAILURE);
    }

    p->in = ins;
    p->out = outs;
    p->err = errs;

    for (n = 0; n < num; n++) {
	p->in[n] = NULL;
	p->in[n] = malloc(PIPE_STREAMS * sizeof(int));
	if (!p->in[n]) {
	    fprintf(stderr, "Error: Could not allocate space to temporary internal pipe array\n");
            c2b_print_usage(stderr);
	    exit(EXIT_FAILURE);
	}
	p->out[n] = NULL;
	p->out[n] = malloc(PIPE_STREAMS * sizeof(int));
	if (!p->out[n]) {
	    fprintf(stderr, "Error: Could not allocate space to temporary internal pipe array\n");
            c2b_print_usage(stderr);
	    exit(EXIT_FAILURE);
	}
	p->err[n] = NULL;
	p->err[n] = malloc(PIPE_STREAMS * sizeof(int));
	if (!p->err[n]) {
	    fprintf(stderr, "Error: Could not allocate space to temporary internal pipe array\n");
            c2b_print_usage(stderr);
	    exit(EXIT_FAILURE);
	}

	/* set close-on-exec flag for each pipe */
	c2b_pipe4_cloexec(p->in[n]);
	c2b_pipe4_cloexec(p->out[n]);
	c2b_pipe4_cloexec(p->err[n]);

        /* set stderr as output for each err write */
        p->err[n][PIPE_WRITE] = fileno(stderr);
    }

    p->num = num;

#ifdef DEBUG
    fprintf(stderr, "--- c2b_init_pipeset() - exit  ---\n");
#endif
}

/* 
   specifying special attribute for c2b_debug_pipeset() to avoid: "warning: unused 
   function 'c2b_debug_pipeset' [-Wunused-function]" message during compilation

   cf. http://gcc.gnu.org/onlinedocs/gcc-3.4.1/gcc/Function-Attributes.html#Function%20Attributes
*/
#if defined(__GNUC__)
static void c2b_debug_pipeset() __attribute__ ((unused));
#endif

static void
c2b_debug_pipeset(c2b_pipeset_t *p, const size_t num)
{
    size_t n;
    size_t s;

    for (n = 0; n < num; n++) {
        for (s = 0; s < PIPE_STREAMS; s++) {
            fprintf(stderr, "\t-> c2b_debug_pipeset - [n, s] = [%zu, %zu] - [%s\t- in, out, err] = [%d, %d, %d]\n",
                    n, 
                    s, 
                    (!s ? "READ" : "WRITE"),
                    p->in[n][s],
                    p->out[n][s],
                    p->err[n][s]);
        }
    }
}

static void
c2b_delete_pipeset(c2b_pipeset_t *p)
{
    size_t n;

    for (n = 0; n < p->num; n++) {
        free(p->in[n]), p->in[n] = NULL;
        free(p->out[n]), p->out[n] = NULL;
        free(p->err[n]), p->err[n] = NULL;
    }
    free(p->in), p->in = NULL;
    free(p->out), p->out = NULL;
    free(p->err), p->err = NULL;

    p->num = 0;
}

static void
c2b_set_close_exec_flag(int fd)
{
    fcntl(fd, F_SETFD, fcntl(fd, F_GETFD) | FD_CLOEXEC);
}

static void
c2b_unset_close_exec_flag(int fd)
{
    fcntl(fd, F_SETFD, fcntl(fd, F_GETFD) & ~FD_CLOEXEC);
}

static int
c2b_pipe4(int fd[2], int flags)
{
    int ret = pipe(fd);
    if (flags & PIPE4_FLAG_RD_CLOEXEC) {
        c2b_set_close_exec_flag(fd[PIPE_READ]);
    }
    if (flags & PIPE4_FLAG_WR_CLOEXEC) {
        c2b_set_close_exec_flag(fd[PIPE_WRITE]);
    }
    return ret;
}

static pid_t
c2b_popen4(const char* cmd, int pin[2], int pout[2], int perr[2], int flags)
{
    pid_t ret = fork();

    if (ret < 0) {
        fprintf(stderr, "fork() failed!\n");
        return ret;
    } 
    else if (ret == 0) {
        /**
         * Child-side of fork
         */
        if (flags & POPEN4_FLAG_CLOSE_CHILD_STDIN) {
            close(STDIN_FILENO);
        }
        else {
            c2b_unset_close_exec_flag(pin[PIPE_READ]);
            dup2(pin[PIPE_READ], STDIN_FILENO);
        }
        if (flags & POPEN4_FLAG_CLOSE_CHILD_STDOUT) {
            close(STDOUT_FILENO);
        }
        else {
            c2b_unset_close_exec_flag(pout[PIPE_WRITE]);
            dup2(pout[PIPE_WRITE], STDOUT_FILENO);
        }
        if (flags & POPEN4_FLAG_CLOSE_CHILD_STDERR) {
            close(STDERR_FILENO);
        }
        else {
            c2b_unset_close_exec_flag(perr[PIPE_WRITE]);
            dup2(perr[PIPE_WRITE], STDERR_FILENO);
        }
        execl("/bin/sh", "/bin/sh", "-c", cmd, NULL);
        fprintf(stderr, "exec() failed!\n");
        exit(EXIT_FAILURE);
    }
    else {
        /**
         * Parent-side of fork
         */
        if (~flags & POPEN4_FLAG_NOCLOSE_PARENT_STDIN && ~flags & POPEN4_FLAG_CLOSE_CHILD_STDIN) {
            close(pin[PIPE_READ]);
        }
        if (~flags & POPEN4_FLAG_NOCLOSE_PARENT_STDOUT && ~flags & POPEN4_FLAG_CLOSE_CHILD_STDOUT) {
            close(pout[PIPE_WRITE]);
        }
        if (~flags & POPEN4_FLAG_NOCLOSE_PARENT_STDERR && ~flags & POPEN4_FLAG_CLOSE_CHILD_STDERR) {
            //close(perr[PIPE_WRITE]);
        }
        return ret;
    }

    /* Unreachable */
    return ret;
}

static void
c2b_test_dependencies()
{
#ifdef DEBUG
    fprintf(stderr, "--- c2b_test_dependencies() - enter ---\n");
#endif

    char *p = NULL;
    char *path = NULL;

    if ((p = getenv("PATH")) == NULL) {
        fprintf(stderr, "Error: Cannot retrieve environment PATH variable\n");
        c2b_print_usage(stderr);
        exit(EXIT_FAILURE);
    }
    path = malloc(strlen(p) + 1);
    if (!path) {
        fprintf(stderr, "Error: Cannot allocate space for path variable copy\n");
        c2b_print_usage(stderr);
        exit(EXIT_FAILURE);
    }
    memcpy(path, p, strlen(p) + 1);

    if ((c2b_globals.input_format_idx == BAM_FORMAT) || (c2b_globals.input_format_idx == SAM_FORMAT)) {
        char *samtools = NULL;
        samtools = malloc(strlen(c2b_samtools) + 1);
        if (!samtools) {
            fprintf(stderr, "Error: Cannot allocate space for samtools variable copy\n");
            c2b_print_usage(stderr);
            exit(EXIT_FAILURE);
        }
        memcpy(samtools, c2b_samtools, strlen(c2b_samtools) + 1);

        char *path_samtools = NULL;
        path_samtools = malloc(strlen(path) + 1);
        if (!path_samtools) {
            fprintf(stderr, "Error: Cannot allocate space for path (samtools) copy\n");
            c2b_print_usage(stderr);
            exit(EXIT_FAILURE);
        }
        memcpy(path_samtools, path, strlen(path) + 1);

#ifdef DEBUG
        fprintf(stderr, "Debug: Searching [%s] for samtools\n", path_samtools);
#endif

        if (c2b_print_matches(path_samtools, samtools) != kTrue) {
            fprintf(stderr, "Error: Cannot find samtools binary required for conversion of BAM and SAM format\n");
            c2b_print_usage(stderr);
            exit(EXIT_FAILURE);
        }
        free(path_samtools), path_samtools = NULL;
        free(samtools), samtools = NULL;
    }

    if (c2b_globals.sort_flag) {
        char *sort_bed = NULL;
        sort_bed = malloc(strlen(c2b_sort_bed) + 1);
        if (!sort_bed) {
            fprintf(stderr, "Error: Cannot allocate space for sort-bed variable copy\n");
            c2b_print_usage(stderr);
            exit(EXIT_FAILURE);
        }
        memcpy(sort_bed, c2b_sort_bed, strlen(c2b_sort_bed) + 1);

        char *path_sort_bed = NULL;
        path_sort_bed = malloc(strlen(path) + 1);
        if (!path_sort_bed) {
            fprintf(stderr, "Error: Cannot allocate space for path (samtools) copy\n");
            c2b_print_usage(stderr);
            exit(EXIT_FAILURE);
        }
        memcpy(path_sort_bed, path, strlen(path) + 1);

#ifdef DEBUG
        fprintf(stderr, "Debug: Searching [%s] for sort-bed\n", path_sort_bed);
#endif

        if (c2b_print_matches(path_sort_bed, sort_bed) != kTrue) {
            fprintf(stderr, "Error: Cannot find sort-bed binary required for sorting BED output\n");
            c2b_print_usage(stderr);
            exit(EXIT_FAILURE);
        }
        free(path_sort_bed), path_sort_bed = NULL;
        free(sort_bed), sort_bed = NULL;
    }

    if (c2b_globals.output_format_idx == STARCH_FORMAT) {
        char *starch = NULL;
        starch = malloc(strlen(c2b_starch) + 1);
        if (!starch) {
            fprintf(stderr, "Error: Cannot allocate space for starch variable copy\n");
            c2b_print_usage(stderr);
            exit(EXIT_FAILURE);
        }
        memcpy(starch, c2b_starch, strlen(c2b_starch) + 1);

        char *path_starch = NULL;
        path_starch = malloc(strlen(path) + 1);
        if (!path_starch) {
            fprintf(stderr, "Error: Cannot allocate space for path (starch) copy\n");
            c2b_print_usage(stderr);
            exit(EXIT_FAILURE);
        }
        memcpy(path_starch, path, strlen(path) + 1);

#ifdef DEBUG
        fprintf(stderr, "Debug: Searching [%s] for starch\n", path_starch);
#endif

        if (c2b_print_matches(path_starch, starch) != kTrue) {
            fprintf(stderr, "Error: Cannot find starch binary required for compression of BED output\n");
            c2b_print_usage(stderr);
            exit(EXIT_FAILURE);
        }
        free(path_starch), path_starch = NULL;
        free(starch), starch = NULL;
    }

    char *cat = NULL;
    cat = malloc(strlen(c2b_cat) + 1);
    if (!cat) {
        fprintf(stderr, "Error: Cannot allocate space for cat variable copy\n");
        c2b_print_usage(stderr);
        exit(EXIT_FAILURE);
    }
    memcpy(cat, c2b_cat, strlen(c2b_cat) + 1);

    char *path_cat = NULL;
    path_cat = malloc(strlen(path) + 1);
    if (!path_cat) {
        fprintf(stderr, "Error: Cannot allocate space for path (cat) copy\n");
        c2b_print_usage(stderr);
        exit(EXIT_FAILURE);
    }
    memcpy(path_cat, path, strlen(path) + 1);

#ifdef DEBUG
        fprintf(stderr, "Debug: Searching [%s] for cat\n", path_cat);
#endif
    
    if (c2b_print_matches(path_cat, cat) != kTrue) {
        fprintf(stderr, "Error: Cannot find cat binary required for piping IO\n");
        c2b_print_usage(stderr);
        exit(EXIT_FAILURE);
    }
    free(path_cat), path_cat = NULL;
    free(cat), cat = NULL;

    free(path), path = NULL;

#ifdef DEBUG
    fprintf(stderr, "--- c2b_test_dependencies() - exit  ---\n");
#endif
}

static boolean
c2b_print_matches(char *path, char *fn)
{
#ifdef DEBUG
    fprintf(stderr, "--- c2b_print_matches() - enter ---\n");
#endif

    char candidate[PATH_MAX];
    const char *d;
    boolean found = kFalse;

    if (strchr(fn, '/') != NULL) {
        return (c2b_is_there(fn) ? kTrue : kFalse);
    }
    while ((d = c2b_strsep(&path, ":")) != NULL) {
        if (*d == '\0') {
            d = ".";
        }
        if (snprintf(candidate, sizeof(candidate), "%s/%s", d, fn) >= (int) sizeof(candidate)) {
            continue;
        }
        if (c2b_is_there(candidate)) {
            found = kTrue;
            if (strcmp(fn, c2b_samtools) == 0) {
                c2b_globals.samtools_path = malloc(strlen(candidate) + 1);
                if (!c2b_globals.samtools_path) {
                    fprintf(stderr, "Error: Could not allocate space for storing samtools path global\n");
                    c2b_print_usage(stderr);
                    exit(EXIT_FAILURE);
                }
                memcpy(c2b_globals.samtools_path, candidate, strlen(candidate) + 1);
            }
            else if (strcmp(fn, c2b_sort_bed) == 0) {
                c2b_globals.sort_bed_path = malloc(strlen(candidate) + 1);
                if (!c2b_globals.sort_bed_path) {
                    fprintf(stderr, "Error: Could not allocate space for storing sortbed path global\n");
                    c2b_print_usage(stderr);
                    exit(EXIT_FAILURE);
                }
                memcpy(c2b_globals.sort_bed_path, candidate, strlen(candidate) + 1);
            }
            else if (strcmp(fn, c2b_starch) == 0) {
                c2b_globals.starch_path = malloc(strlen(candidate) + 1);
                if (!c2b_globals.starch_path) {
                    fprintf(stderr, "Error: Could not allocate space for storing starch path global\n");
                    c2b_print_usage(stderr);
                    exit(EXIT_FAILURE);
                }
                memcpy(c2b_globals.starch_path, candidate, strlen(candidate) + 1);
            }
            else if (strcmp(fn, c2b_cat) == 0) {
                c2b_globals.cat_path = malloc(strlen(candidate) + 1);
                if (!c2b_globals.cat_path) {
                    fprintf(stderr, "Errrpr: Could not allocate space for storing cat path global\n");
                    exit(EXIT_FAILURE);
                }
                memcpy(c2b_globals.cat_path, candidate, strlen(candidate) + 1);
            }
            break;
        }
    }

#ifdef DEBUG
    fprintf(stderr, "--- c2b_print_matches() - exit  ---\n");
#endif

    return found;
}

static char *
c2b_strsep(char **stringp, const char *delim)
{
    char *s;
    const char *spanp;
    int c, sc;
    char *tok;

    if ((s = *stringp) == NULL)
        return NULL;

    for (tok = s;;) {
        c = *s++;
        spanp = delim;
        do {
            if ((sc = *spanp++) == c) {
                if (c == 0)
                    s = NULL;
                else
                    s[-1] = 0;
                *stringp = s;
                return tok;
            }
        } while (sc != 0);
    }

    return NULL;
}

static boolean
c2b_is_there(char *candidate)
{
#ifdef DEBUG
    fprintf(stderr, "--- c2b_is_there() - enter ---\n");
#endif

    struct stat fin;
    boolean found = kFalse;

    if (access(candidate, X_OK) == 0 
        && stat(candidate, &fin) == 0 
        && S_ISREG(fin.st_mode) 
        && (getuid() != 0 || (fin.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)) != 0)) {
#ifdef DEBUG
	    fprintf(stderr, "Debug: Found dependency [%s]\n", candidate);
#endif
        found = kTrue;
    }

#ifdef DEBUG
    fprintf(stderr, "--- c2b_is_there() - exit  ---\n");
#endif

    return found;
}

static void
c2b_init_globals()
{
#ifdef DEBUG
    fprintf(stderr, "--- c2b_init_globals() - enter ---\n");
#endif

    c2b_globals.input_format = NULL;
    c2b_globals.input_format_idx = UNDEFINED_FORMAT;
    c2b_globals.output_format = NULL;
    c2b_globals.output_format_idx = UNDEFINED_FORMAT;
    c2b_globals.samtools_path = NULL;
    c2b_globals.sort_bed_path = NULL;
    c2b_globals.starch_path = NULL;
    c2b_globals.cat_path = NULL;
    c2b_globals.sort_flag = kTrue;
    c2b_globals.all_reads_flag = kFalse;
    c2b_globals.keep_header_flag = kFalse;
    c2b_globals.split_flag = kFalse;
    c2b_globals.headered_flag = kFalse;
    c2b_globals.vcf_snvs_flag = kFalse;
    c2b_globals.vcf_insertions_flag = kFalse;
    c2b_globals.vcf_deletions_flag = kFalse;
    c2b_globals.header_line_idx = 0U;
    c2b_globals.starch_bzip2_flag = kFalse;
    c2b_globals.starch_gzip_flag = kFalse;
    c2b_globals.starch_note = NULL;
    c2b_globals.max_mem_value = NULL;
    c2b_globals.sort_tmpdir_path = NULL;
    c2b_globals.wig_basename = NULL;
    c2b_globals.cigar = NULL, c2b_sam_init_cigar_ops(&c2b_globals.cigar, MAX_OPERATIONS_VALUE);
    c2b_globals.gff_id = NULL;
    c2b_globals.gtf_id = NULL;

#ifdef DEBUG
    fprintf(stderr, "--- c2b_init_globals() - exit  ---\n");
#endif
}

static void
c2b_delete_globals()
{
#ifdef DEBUG
    fprintf(stderr, "--- c2b_delete_globals() - enter ---\n");
#endif

    if (c2b_globals.input_format)
        free(c2b_globals.input_format), c2b_globals.input_format = NULL;
    if (c2b_globals.samtools_path)
        free(c2b_globals.samtools_path), c2b_globals.samtools_path = NULL;
    if (c2b_globals.sort_bed_path)
        free(c2b_globals.sort_bed_path), c2b_globals.sort_bed_path = NULL;
    if (c2b_globals.starch_path)
        free(c2b_globals.starch_path), c2b_globals.starch_path = NULL;
    if (c2b_globals.cat_path)
        free(c2b_globals.cat_path), c2b_globals.cat_path = NULL;
    c2b_globals.input_format_idx = UNDEFINED_FORMAT;
    c2b_globals.sort_flag = kTrue;
    c2b_globals.all_reads_flag = kFalse;
    c2b_globals.keep_header_flag = kFalse;
    c2b_globals.split_flag = kFalse;
    c2b_globals.headered_flag = kFalse;
    c2b_globals.vcf_snvs_flag = kFalse;
    c2b_globals.vcf_insertions_flag = kFalse;
    c2b_globals.vcf_deletions_flag = kFalse;
    c2b_globals.header_line_idx = 0U;
    c2b_globals.starch_bzip2_flag = kFalse;
    c2b_globals.starch_gzip_flag = kFalse;
    if (c2b_globals.starch_note)
        free(c2b_globals.starch_note), c2b_globals.starch_note = NULL;
    if (c2b_globals.max_mem_value)
        free(c2b_globals.max_mem_value), c2b_globals.max_mem_value = NULL;
    if (c2b_globals.sort_tmpdir_path)
        free(c2b_globals.sort_tmpdir_path), c2b_globals.sort_tmpdir_path = NULL;
    if (c2b_globals.wig_basename)
        free(c2b_globals.wig_basename), c2b_globals.wig_basename = NULL;
    if (c2b_globals.cigar)
        c2b_sam_delete_cigar_ops(c2b_globals.cigar);
    if (c2b_globals.gff_id)
        free(c2b_globals.gff_id), c2b_globals.gff_id = NULL;
    if (c2b_globals.gtf_id)
        free(c2b_globals.gtf_id), c2b_globals.gtf_id = NULL;

#ifdef DEBUG
    fprintf(stderr, "--- c2b_delete_globals() - exit  ---\n");
#endif
}

static void
c2b_init_command_line_options(int argc, char **argv)
{
#ifdef DEBUG
    fprintf(stderr, "--- c2b_init_command_line_options() - enter ---\n");
#endif

    char *input_format = NULL;
    char *output_format = NULL;
    int client_long_index;
    int client_opt = getopt_long(argc,
                                 argv,
                                 c2b_client_opt_string,
                                 c2b_client_long_options,
                                 &client_long_index);

    opterr = 0; /* disable error reporting by GNU getopt */

    while (client_opt != -1) {
        switch (client_opt) 
            {
            case 'i':
                input_format = malloc(strlen(optarg) + 1);
                if (!input_format) {
                    fprintf(stderr, "Error: Could not allocate space for input format argument\n");
                    c2b_print_usage(stderr);
                    exit(EXIT_FAILURE);
                }
                memcpy(input_format, optarg, strlen(optarg) + 1);
                c2b_globals.input_format = c2b_to_lowercase(input_format);
                c2b_globals.input_format_idx = c2b_to_input_format(c2b_globals.input_format);
                free(input_format), input_format = NULL;
                break;
            case 'o':
                output_format = malloc(strlen(optarg) + 1);
                if (!output_format) {
                    fprintf(stderr, "Error: Could not allocate space for output format argument\n");
                    c2b_print_usage(stderr);
                    exit(EXIT_FAILURE);
                }
                memcpy(output_format, optarg, strlen(optarg) + 1);
                c2b_globals.output_format = c2b_to_lowercase(output_format);
                c2b_globals.output_format_idx = c2b_to_output_format(c2b_globals.output_format);
                free(output_format), output_format = NULL;
                break;
            case 'm':
                c2b_globals.max_mem_value = malloc(strlen(optarg) + 1);
                if (!c2b_globals.max_mem_value) {
                    fprintf(stderr, "Error: Could not allocate space for sort-bed max-mem argument\n");
                    c2b_print_usage(stderr);
                    exit(EXIT_FAILURE);
                }
                memcpy(c2b_globals.max_mem_value, optarg, strlen(optarg) + 1);
                break;
            case 'r':
                c2b_globals.sort_tmpdir_path = malloc(strlen(optarg) + 1);
                if (!c2b_globals.sort_tmpdir_path) {
                    fprintf(stderr, "Error: Could not allocate space for sort-bed temporary directory argument\n");
                    c2b_print_usage(stderr);
                    exit(EXIT_FAILURE);
                }
                memcpy(c2b_globals.sort_tmpdir_path, optarg, strlen(optarg) + 1);
                break;
            case 'e':
                c2b_globals.starch_note = malloc(strlen(optarg) + 1);
                if (!c2b_globals.starch_note) {
                    fprintf(stderr, "Error: Could not allocate space for Starch note\n");
                    c2b_print_usage(stderr);
                    exit(EXIT_FAILURE);
                }
                memcpy(c2b_globals.starch_note, optarg, strlen(optarg) + 1);
                break;
            case 's':
                c2b_globals.split_flag = kTrue;
                break;
            case 'd':
                c2b_globals.sort_flag = kFalse;
                break;
            case 'a':
                c2b_globals.all_reads_flag = kTrue;
                break;
            case 'p':
                c2b_globals.headered_flag = kTrue;
                break;
            case 'k':
                c2b_globals.keep_header_flag = kTrue;
                break;
            case 'z':
                c2b_globals.starch_bzip2_flag = kTrue;
                break;
            case 'g':
                c2b_globals.starch_gzip_flag = kTrue;
                break;
            case 'h':
                c2b_print_usage(stdout);
                exit(EXIT_SUCCESS);
            case '?':
                c2b_print_usage(stderr);
                exit(EXIT_SUCCESS);
            default:
                break;
        }
        client_opt = getopt_long(argc,
                                 argv,
                                 c2b_client_opt_string,
                                 c2b_client_long_options,
                                 &client_long_index);
    }

    if ((!c2b_globals.input_format) || (c2b_globals.input_format_idx == UNDEFINED_FORMAT)) {
        c2b_print_usage(stderr);
        exit(EXIT_FAILURE);
    }

    if ((!c2b_globals.output_format) || (c2b_globals.output_format_idx == UNDEFINED_FORMAT)) {
        c2b_globals.output_format = malloc(strlen(c2b_default_output_format) + 1);
        if (!c2b_globals.output_format) {
            fprintf(stderr, "Error: Could not allocate space for output format copy\n");
            c2b_print_usage(stderr);
            exit(EXIT_FAILURE);
        }
        memcpy(c2b_globals.output_format, c2b_default_output_format, strlen(c2b_default_output_format) + 1);
        c2b_globals.output_format_idx = c2b_to_output_format(c2b_globals.output_format);
    }

    if (c2b_globals.input_format_idx == GFF_FORMAT) {
        c2b_globals.gff_id = malloc(MAX_FIELD_LENGTH_VALUE);
        if (!c2b_globals.gff_id) {
            fprintf(stderr, "Error: Could not allocate space for GFF ID global\n");
            exit(EXIT_FAILURE);
        }
        memset(c2b_globals.gff_id, 0, MAX_FIELD_LENGTH_VALUE);
    }

    if (c2b_globals.input_format_idx == GTF_FORMAT) {
        c2b_globals.gtf_id = malloc(MAX_FIELD_LENGTH_VALUE);
        if (!c2b_globals.gtf_id) {
            fprintf(stderr, "Error: Could not allocate space for GTF ID global\n");
            exit(EXIT_FAILURE);
        }
        memset(c2b_globals.gtf_id, 0, MAX_FIELD_LENGTH_VALUE);
    }

    if ((c2b_globals.starch_bzip2_flag) && (c2b_globals.starch_gzip_flag)) {
        fprintf(stderr, "Error: Cannot specify both Starch compression options\n");
        c2b_print_usage(stderr);
        exit(EXIT_FAILURE);
    }

    if (!(c2b_globals.starch_bzip2_flag) && !(c2b_globals.starch_gzip_flag) && (c2b_globals.output_format_idx == STARCH_FORMAT)) {
        c2b_globals.starch_bzip2_flag = kTrue;
    }
    else if ((c2b_globals.starch_bzip2_flag || c2b_globals.starch_gzip_flag) && (c2b_globals.output_format_idx == BED_FORMAT)) {
        fprintf(stderr, "Error: Cannot specify Starch compression options without setting output format to Starch\n");
        c2b_print_usage(stderr);
        exit(EXIT_FAILURE);
    }

#ifdef DEBUG
    fprintf(stderr, "--- c2b_init_command_line_options() - exit  ---\n");
#endif
}

static void
c2b_print_usage(FILE *stream)
{
#ifdef DEBUG
    fprintf(stderr, "--- c2b_print_usage() - enter ---\n");
#endif

    fprintf(stream,
            "%s\n" \
            "  version: %s\n" \
            "  author:  %s\n" \
            "%s\n",
            name,
            version,
            authors,
            usage);

#ifdef DEBUG
    fprintf(stderr, "--- c2b_print_usage() - exit  ---\n");
#endif
}

static char *
c2b_to_lowercase(const char *src)
{
#ifdef DEBUG
    fprintf(stderr, "--- c2b_to_lowercase() - enter ---\n");
#endif

    char *dest = NULL;
    char *p = NULL;

    if (!src)
        return dest;

    p = malloc(strlen(src) + 1);
    if (!p) {
        fprintf(stderr, "Error: Could not allocate space for lowercase translation\n");
        c2b_print_usage(stderr);
        exit(EXIT_FAILURE);
    }
    memcpy(p, src, strlen(src) + 1);
    dest = p;
    for ( ; *p; ++p)
        *p = (*p >= 'A' && *p <= 'Z') ? (*p | 0x60) : *p;

#ifdef DEBUG
    fprintf(stderr, "--- c2b_to_lowercase() - exit  ---\n");
#endif
    return dest;
}

static c2b_format_t
c2b_to_input_format(const char *input_format)
{
#ifdef DEBUG
    fprintf(stderr, "--- c2b_to_input_format() - enter ---\n");
    fprintf(stderr, "--- c2b_to_input_format() - exit  ---\n");
#endif

    return
        (strcmp(input_format, "bam") == 0) ? BAM_FORMAT :
        (strcmp(input_format, "gff") == 0) ? GFF_FORMAT :
        (strcmp(input_format, "gtf") == 0) ? GTF_FORMAT :
        (strcmp(input_format, "psl") == 0) ? PSL_FORMAT :
        (strcmp(input_format, "sam") == 0) ? SAM_FORMAT :
        (strcmp(input_format, "vcf") == 0) ? VCF_FORMAT :
        (strcmp(input_format, "wig") == 0) ? WIG_FORMAT :
        UNDEFINED_FORMAT;
}

static c2b_format_t
c2b_to_output_format(const char *output_format)
{
#ifdef DEBUG
    fprintf(stderr, "--- c2b_to_output_format() - enter ---\n");
    fprintf(stderr, "--- c2b_to_output_format() - exit  ---\n");
#endif

    return
        (strcmp(output_format, "bed") == 0) ? BED_FORMAT :
        (strcmp(output_format, "starch") == 0) ? STARCH_FORMAT :
        UNDEFINED_FORMAT;
}
