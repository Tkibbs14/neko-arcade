#include "live.h"
#include "nekolm.h"
#include "lines.h"

int live_ban_he;
int live_require_approved = 1;
u32 live_stat_composed, live_stat_whole;

void live_init(LiveJob *j, int spk, int sit, const char *key, const char *value)
{
    j->spk = (i16)spk;
    j->sit = (i16)sit;
    j->key = key;
    int n = 0;
    for (; value && value[n] && n < (int)sizeof j->value - 1; n++)
        j->value[n] = value[n];
    j->value[n] = 0;
    j->out[0] = 0;
    j->state = LIVE_PENDING;
    j->ticket = 0;
    j->pool = 0;
}

static int says_he(const char *s)
{
    static const char *const words[] = { "he", "He", "him", "Him", "his", "His" };
    for (int i = 0; s[i]; i++) {
        if (i > 0 && s[i - 1] != ' ' && s[i - 1] != '"')
            continue;
        for (int w = 0; w < 6; w++) {
            int n = str_len(words[w]), k = 0;
            while (k < n && s[i + k] == words[w][k]) k++;
            if (k == n) {
                char c = s[i + n];
                if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')))
                    return 1;
            }
        }
    }
    return 0;
}

static const CPool *find_pool(int spk, int sit)
{
    for (int i = 0; i < cpool_count; i++)
        if (cpools[i].spk == spk && cpools[i].sit == sit)
            return &cpools[i];
    return 0;
}

/* Where the bank has approved pairs (combinations three large models all rated fully sensible), pick one at
 * random and offer NekoLM up to LIVE_CANDS of that opener's approved continuations. */
static int choose_approved(LiveJob *j, const CPool *p)
{
    for (int tries = 0; tries < 12; tries++) {
        int i = (int)(rnd() % p->nok), o = p->ok[i] >> 8;
        if (live_ban_he && says_he(p->op[o].text))
            continue;
        int lo = i, hi = i;                          /* the list is sorted, so the opener's pairs are together */
        while (lo > 0 && (p->ok[lo - 1] >> 8) == o) lo--;
        while (hi + 1 < p->nok && (p->ok[hi + 1] >> 8) == o) hi++;
        int n = 0, seen = 0;
        for (int k = lo; k <= hi; k++) {
            int r = p->ok[k] & 255;
            if (live_ban_he && says_he(p->rest[r].text))
                continue;
            seen++;
            if (n < LIVE_CANDS) j->cand[n++] = (i16)r;
            else {
                int x = (int)(rnd() % (u32)seen);
                if (x < LIVE_CANDS) j->cand[x] = (i16)r;
            }
        }
        if (n) {
            j->pool = p;
            j->op = (i16)o;
            j->ncand = (i16)n;
            return 1;
        }
    }
    return 0;
}

/* Pick an opener and up to LIVE_CANDS rests that can follow it: from another teacher line, fitting the dialogue
 * box and the model's context, and keeping exactly one placeholder when the situation has one. With approved
 * pairs in the bank only those are used; a situation without any gets a whole teacher line instead. */
static int choose(LiveJob *j)
{
    const CPool *p = find_pool(j->spk, j->sit);
    if (!p || !p->nop || !p->nrest)
        return 0;
    if (p->nok)
        return choose_approved(j, p);
    if (live_require_approved)
        return 0;
    for (int tries = 0; tries < 12; tries++) {
        int o = (int)(rnd() % p->nop);
        const CUnit *op = &p->op[o];
        if (live_ban_he && says_he(op->text))
            continue;
        int n = 0, seen = 0;
        for (int r = 0; r < p->nrest; r++) {
            const CUnit *u = &p->rest[r];
            if (u->src == op->src || op->len + 1 + u->len > NLM_MAXLEN || op->ntok + u->ntok + 4 > NLM_T)
                continue;
            if (p->key ? op->key + u->key != 1 : op->key || u->key)
                continue;
            if (live_ban_he && says_he(u->text))
                continue;
            seen++;                                   /* reservoir sampling keeps the choice uniform */
            if (n < LIVE_CANDS) j->cand[n++] = (i16)r;
            else {
                int k = (int)(rnd() % (u32)seen);
                if (k < LIVE_CANDS) j->cand[k] = (i16)r;
            }
        }
        if (n) {
            j->pool = p;
            j->op = (i16)o;
            j->ncand = (i16)n;
            return 1;
        }
    }
    return 0;
}

/* One slice of the composer's work: feed the prefix (line start, speaker, situation, opener), then each
 * candidate rest from the same point, adding up NekoLM's log-probability of every token and the line's end. */
static int compose_work(void *ctx)
{
    LiveJob *j = ctx;
    if (nlm_busy()) {
        nlm_slice();
        return 0;
    }
    const CPool *p = j->pool;
    if (j->stage == 0) {
        const CUnit *op = &p->op[j->op];
        if (j->ti < op->ntok + 3) {
            int tok = j->ti == 0 ? 10 : j->ti == 1 ? j->spk : j->ti == 2 ? j->sit : op->tok[j->ti - 3];
            j->ti++;
            nlm_feed(tok);
            return 0;
        }
        j->prefix_pos = (i16)nlm_cursor();
        for (int k = 0; k < j->ncand; k++)
            j->score[k] = nlm_logprob_q8(p->rest[j->cand[k]].tok[0]);
        j->stage = 1;
        j->ci = 0;
        j->ti = 0;
        return 0;
    }
    if (j->stage == 1) {
        const CUnit *r = &p->rest[j->cand[j->ci]];
        if (j->ti > 0)                               /* the logits now predict the token after tok[ti - 1] */
            j->score[j->ci] += nlm_logprob_q8(j->ti < r->ntok ? r->tok[j->ti] : 10);
        if (j->ti < r->ntok) {
            if (j->ti == 0)
                nlm_rewind(j->prefix_pos);
            nlm_feed(r->tok[j->ti++]);
            return 0;
        }
        j->score[j->ci] /= r->ntok + 1;              /* mean per token, so length does not decide */
        if (++j->ci < j->ncand) {
            j->ti = 0;
            return 0;
        }
        j->stage = 2;
    }
    return 1;
}

static void finish_composed(LiveJob *j)
{
    const CPool *p = j->pool;
    int best = 0;
    for (int k = 1; k < j->ncand; k++)
        if (j->score[k] > j->score[best]) best = k;
    char line[NLM_MAXLEN + 2];
    char *e = str_cpy(line, p->op[j->op].text);
    *e++ = ' ';
    str_cpy(e, p->rest[j->cand[best]].text);
    nlm_fill(j->out, line, j->key ? j->key : "", j->value);
    live_stat_composed++;
}

static void finish_whole(LiveJob *j)
{
    const char *fb = 0;
    for (int tries = 0; tries < 8; tries++) {
        fb = line_pick(j->spk, j->sit);
        if (!fb || !(live_ban_he && says_he(fb)))
            break;
    }
    nlm_fill(j->out, fb ? fb : "...", j->key ? j->key : "", j->value);
    live_stat_whole++;
}

int live_step(LiveJob *j, int max_us, int max_slices)
{
    (void)max_us;
    if (j->state == LIVE_DONE)
        return 1;
    if (j->state == LIVE_RUNNING && nlm_current() != j->ticket)
        j->state = LIVE_PENDING;                     /* someone else used the model meanwhile: start over */
    if (j->state == LIVE_PENDING) {
        if (!j->pool && !choose(j)) {                /* nothing composes here: a whole teacher line */
            finish_whole(j);
            j->state = LIVE_DONE;
            return 1;
        }
        if (j->ncand == 1) {                         /* one continuation: nothing for NekoLM to choose between */
            finish_composed(j);
            j->state = LIVE_DONE;
            return 1;
        }
        j->ticket = nlm_claim();
        j->stage = 0;
        j->ti = 0;
        j->state = LIVE_RUNNING;
    }
    if (!nlm_work_for(compose_work, j, max_slices))
        return 0;
    finish_composed(j);
    j->state = LIVE_DONE;
    return 1;
}
